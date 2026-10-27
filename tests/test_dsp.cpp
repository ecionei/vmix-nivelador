// Testes do processamento (sem JUCE). Compila com: g++ -O2 -std=c++17 -I../Source test_dsp.cpp
#include "LevelerDSP.h"
#include <cstdio>
#include <random>
#include <string>

using namespace nivelador;

static int failures = 0;
static void check (bool ok, const std::string& what)
{
    std::printf ("%s  %s\n", ok ? "OK  " : "FALHOU", what.c_str());
    if (! ok) ++failures;
}

// Pico entre amostras medido com 8x (sinc longo) para conferir o limitador.
static double truePeak (const std::vector<float>& x)
{
    const int half = 32, over = 8;
    static double h[over][2 * half];
    static bool init = false;
    if (! init)
    {
        for (int k = 0; k < over; ++k)
            for (int i = -half + 1; i <= half; ++i)
            {
                const double t = (double) k / over - i;
                const double s = std::abs (t) < 1e-12 ? 1.0 : std::sin (kPi * t) / (kPi * t);
                const double w = 0.5 + 0.5 * std::cos (kPi * t / half);
                h[k][i + half - 1] = s * w;
            }
        init = true;
    }
    double peak = 0.0;
    for (size_t n = (size_t) half; n + (size_t) half < x.size(); ++n)
        for (int k = 0; k < over; ++k)
        {
            double v = 0.0;
            const float* p = &x[n - (size_t) half + 1];
            for (int i = 0; i < 2 * half; ++i) v += p[i] * h[k][i];
            peak = std::max (peak, std::abs (v));
        }
    return peak;
}

static double loudnessOf (const std::vector<float>& L, const std::vector<float>& R, double fs, size_t from, size_t to)
{
    LoudnessMeter m;
    m.prepare (fs, 2, (double) (to - from) / fs);
    m.setGate (-70.0);
    for (size_t i = from; i < to; ++i)
    {
        float f[2] = { L[i], R[i] };
        m.pushFrame (f, 2);
    }
    return m.getLoudness();
}

// "Voz" sintética: ruído rosa-ish com sílabas (envelope 4 Hz) e pausas.
static void makeSpeech (std::vector<float>& L, std::vector<float>& R, size_t from, size_t to, double fs, double gain, unsigned seed)
{
    std::mt19937 rng (seed);
    std::normal_distribution<double> nd (0.0, 1.0);
    double b0 = 0, b1 = 0;
    for (size_t i = from; i < to; ++i)
    {
        const double t = (double) i / fs;
        double w = nd (rng);
        b0 = 0.97 * b0 + 0.03 * w; b1 = 0.6 * b1 + 0.4 * w;
        const double env = std::pow (std::max (0.0, std::sin (2 * kPi * 4.0 * t)), 2.0);
        const double v = gain * (b0 * 3.0 + b1 * 0.5) * env;
        L[i] = (float) v; R[i] = (float) v;
    }
}

int run();
int main()
{ setvbuf (stdout, nullptr, _IONBF, 0); return run(); }
int run()
{
    const double fs = 48000.0;

    // 1) Medidor: seno de 997 Hz a -20 dBFS nos dois canais = -20 LUFS
    {
        const size_t N = (size_t) (fs * 3);
        std::vector<float> L (N), R (N);
        const double a = dbToLin (-20.0);
        for (size_t i = 0; i < N; ++i) L[i] = R[i] = (float) (a * std::sin (2 * kPi * 997.0 * (double) i / fs));
        const double lu = loudnessOf (L, R, fs, 0, N);
        char buf[128]; std::snprintf (buf, sizeof buf, "medidor: seno -20 dBFS => %.2f LUFS (esperado -20.0)", lu);
        check (std::abs (lu + 20.0) < 0.1, buf);
    }

    // 2) Nivelador: voz baixa (~-32 LUFS) depois alta (~-6 LUFS), alvo -14
    for (int mode = 0; mode < 2; ++mode)
    {
        const size_t seg = (size_t) (fs * 20);
        const size_t N = seg * 3;
        std::vector<float> L (N, 0.0f), R (N, 0.0f);
        makeSpeech (L, R, 0, seg, fs, 0.06, 1);
        makeSpeech (L, R, seg, 2 * seg, fs, 0.8, 2);
        // terceiro segmento: só ruído de fundo bem baixo (silêncio de estúdio)
        std::mt19937 rng (3); std::normal_distribution<double> nd (0.0, 1.0);
        for (size_t i = 2 * seg; i < N; ++i) L[i] = R[i] = (float) (dbToLin (-70.0) * nd (rng));

        const double inLow = loudnessOf (L, R, fs, seg / 2, seg);
        const double inHigh = loudnessOf (L, R, fs, seg + seg / 2, 2 * seg);
        const double inNoise = loudnessOf (L, R, fs, 2 * seg + seg / 2, N);

        Leveler lev;
        Settings s; s.mode = mode; s.targetLufs = -14.0; s.maxBoostDb = 24.0;
        lev.setSettings (s);
        lev.prepare (fs, 2, 512);
        for (size_t i = 0; i < N; i += 512)
        {
            const int n = (int) std::min<size_t> (512, N - i);
            float* ch[2] = { &L[i], &R[i] };
            lev.process (ch, 2, n);
        }
        const size_t lat = (size_t) lev.getLatency();
        const double outLow = loudnessOf (L, R, fs, seg / 2 + lat, seg + lat);
        const double outHigh = loudnessOf (L, R, fs, seg + seg / 2 + lat, 2 * seg + lat);
        const double outNoise = loudnessOf (L, R, fs, 2 * seg + seg / 2 + lat, N);
        const double tp = linToDb (truePeak (L));
        const char* name = mode == 0 ? "Voz" : "Programa";
        char buf[200];
        std::snprintf (buf, sizeof buf, "[%s] voz baixa %.1f -> %.1f LUFS (alvo -14)", name, inLow, outLow);
        check (std::abs (outLow + 14.0) < 1.5, buf);
        std::snprintf (buf, sizeof buf, "[%s] voz alta %.1f -> %.1f LUFS (alvo -14)", name, inHigh, outHigh);
        check (std::abs (outHigh + 14.0) < 1.5, buf);
        std::snprintf (buf, sizeof buf, "[%s] silêncio %.1f -> %.1f LUFS (não pode subir mais que o ganho mantido)", name, inNoise, outNoise);
        check (outNoise < inNoise + 12.0, buf);
        std::snprintf (buf, sizeof buf, "[%s] true peak da saída %.2f dBTP (teto -1)", name, tp);
        check (tp <= -0.8, buf);
    }

    // 3) Limitador sozinho: seno perto de fs/4 com pico entre amostras
    {
        const size_t N = (size_t) fs;
        std::vector<float> L (N), R (N);
        for (size_t i = 0; i < N; ++i)
            L[i] = R[i] = (float) (1.5 * std::sin (2 * kPi * 11025.0 * (double) i / fs + 0.785));
        Leveler lev; Settings s; s.levelerOn = false; s.ceilingDbtp = -1.0;
        lev.setSettings (s); lev.prepare (fs, 2, 256);
        float* ch[2] = { L.data(), R.data() };
        lev.process (ch, 2, (int) N);
        const double tp = linToDb (truePeak (L));
        char buf[128]; std::snprintf (buf, sizeof buf, "limitador: seno +3.5 dBFS 11 kHz -> %.2f dBTP (teto -1)", tp);
        check (tp <= -0.7, buf);
    }

    std::printf (failures ? "\n%d teste(s) falharam\n" : "\nTodos os testes passaram\n", failures);
    return failures ? 1 : 0;
}
