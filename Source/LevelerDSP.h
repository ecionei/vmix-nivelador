// Nivelador: AGC por loudness (LUFS, ITU-R BS.1770) + limitador true-peak com lookahead.
// Sem dependência do JUCE, para poder ser testado sozinho.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>

namespace nivelador
{

constexpr double kPi = 3.14159265358979323846;

inline double dbToLin (double db) { return std::pow (10.0, db / 20.0); }
inline double linToDb (double lin) { return 20.0 * std::log10 (std::max (lin, 1.0e-12)); }

//==============================================================================
// Biquad simples (forma direta I), em double.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;

    void reset() { x1 = x2 = y1 = y2 = 0; }

    double process (double x)
    {
        const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }
};

//==============================================================================
// Filtro K (BS.1770) para qualquer taxa de amostragem.
struct KWeighting
{
    Biquad shelf, highpass;

    void prepare (double fs)
    {
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (kPi * f0 / fs);
            const double Vh = std::pow (10.0, G / 20.0);
            const double Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf.b0 = (Vh + Vb * K / Q + K * K) / a0;
            shelf.b1 = 2.0 * (K * K - Vh) / a0;
            shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
            shelf.a1 = 2.0 * (K * K - 1.0) / a0;
            shelf.a2 = (1.0 - K / Q + K * K) / a0;
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (kPi * f0 / fs);
            const double a0 = 1.0 + K / Q + K * K;
            highpass.b0 = 1.0; highpass.b1 = -2.0; highpass.b2 = 1.0;
            highpass.a1 = 2.0 * (K * K - 1.0) / a0;
            highpass.a2 = (1.0 - K / Q + K * K) / a0;
        }
        reset();
    }

    void reset() { shelf.reset(); highpass.reset(); }
    double process (double x) { return highpass.process (shelf.process (x)); }
};

//==============================================================================
// Medidor de loudness em janela deslizante, em blocos de 50 ms.
// Com gate ligado, blocos abaixo do gate (silêncio, pausas, ruído) ficam fora da
// média: assim pausas na fala não fazem o nivelador subir o ganho.
class LoudnessMeter
{
public:
    static constexpr double kBlockSeconds = 0.05;

    void prepare (double fs, int numChannels, double windowSeconds)
    {
        filters.assign ((size_t) std::max (1, numChannels), KWeighting());
        for (auto& f : filters) f.prepare (fs);
        blockLen = std::max (1, (int) std::lround (fs * kBlockSeconds));
        blocks.assign ((size_t) kMaxBlocks, 0.0);
        counted.assign ((size_t) kMaxBlocks, 0);
        numBlocks = 0;
        setWindow (windowSeconds);
        reset();
    }

    static constexpr int kMaxBlocks = 200; // até 10 s

    // Não aloca memória (pode ser chamado na thread de áudio).
    void setWindow (double windowSeconds)
    {
        const int n = std::clamp ((int) std::lround (windowSeconds / kBlockSeconds), 1, kMaxBlocks);
        if (n == numBlocks) return;
        numBlocks = n;
        std::fill (blocks.begin(), blocks.end(), 0.0);
        std::fill (counted.begin(), counted.end(), 0);
        writePos = 0;
    }

    // Blocos com loudness abaixo disso não entram na média (-200 = sem gate).
    void setGate (double lufs) { gateEnergy = std::pow (10.0, (lufs + 0.691) / 10.0); }

    void reset()
    {
        for (auto& f : filters) f.reset();
        std::fill (blocks.begin(), blocks.end(), 0.0);
        std::fill (counted.begin(), counted.end(), 0);
        writePos = 0; acc = 0.0; accCount = 0;
        loudness = -120.0; hasSignal = false; lastBlock = -120.0;
    }

    // Retorna true quando um bloco fecha (loudness atualizado).
    bool pushFrame (const float* frame, int numChannels)
    {
        double e = 0.0;
        const int nc = std::min (numChannels, (int) filters.size());
        for (int c = 0; c < nc; ++c)
        {
            const double y = filters[(size_t) c].process ((double) frame[c]);
            e += y * y; // peso 1.0 para L e R (BS.1770)
        }
        acc += e;
        lastEnergy = e;
        if (++accCount < blockLen) return false;

        const double ms = acc / (double) accCount;
        acc = 0.0; accCount = 0;
        lastBlock = ms > 1.0e-13 ? -0.691 + 10.0 * std::log10 (ms) : -120.0;

        blocks[(size_t) writePos] = ms;
        counted[(size_t) writePos] = ms > gateEnergy ? 1 : 0;
        writePos = (writePos + 1) % numBlocks;

        double sum = 0.0; int n = 0;
        for (int i = 0; i < numBlocks; ++i)
            if (counted[(size_t) i]) { sum += blocks[(size_t) i]; ++n; }

        hasSignal = n > 0;
        fraction = (double) n / (double) numBlocks;
        loudness = hasSignal ? -0.691 + 10.0 * std::log10 (std::max (sum / n, 1.0e-13)) : -120.0;
        return true;
    }

    double getLoudness() const { return loudness; }
    double getLastBlockLoudness() const { return lastBlock; }
    double getLastFrameEnergy() const { return lastEnergy; } // filtro K, soma dos canais
    bool   hasSignalAboveGate() const { return hasSignal; }
    double fractionAboveGate() const { return fraction; }

private:
    std::vector<KWeighting> filters;
    std::vector<double> blocks;
    std::vector<char> counted;
    int blockLen = 2400, numBlocks = 0, writePos = 0, accCount = 0;
    double acc = 0.0, gateEnergy = 0.0, loudness = -120.0, lastBlock = -120.0, lastEnergy = 0.0;
    bool hasSignal = false;
    double fraction = 0.0;
};

//==============================================================================
// Limitador brickwall com lookahead e detecção de pico entre amostras (8x).
class TruePeakLimiter
{
public:
    static constexpr int kTaps = 32;           // taps por fase
    static constexpr int kDetectDelay = kTaps / 2;
    static constexpr int kOver = 8;            // detector de pico 8x

    void prepare (double fs, int numChannels)
    {
        sampleRate = fs;
        nch = std::max (1, numChannels);
        lookahead = std::max (1, (int) std::lround (fs * 0.0015));
        totalDelay = (lookahead - 1) + kDetectDelay;

        // Coeficientes de interpolação (sinc janelado com Blackman).
        for (int k = 0; k < kOver; ++k)
            for (int i = 0; i < kTaps; ++i)
            {
                // amostra i está em (t0 - (kDetectDelay-1) + i); ponto interpolado em (t0 + k/kOver)
                const double tpos = (double) (kDetectDelay - 1 - i) + (double) k / kOver;
                const double s = std::abs (tpos) < 1e-12 ? 1.0 : std::sin (kPi * tpos) / (kPi * tpos);
                const double wpos = (tpos + kDetectDelay) / (2.0 * kDetectDelay); // 0..1 sobre a janela
                const double w = 0.42 - 0.5 * std::cos (2.0 * kPi * wpos) + 0.08 * std::cos (4.0 * kPi * wpos);
                coef[k][i] = s * std::max (w, 0.0);
            }

        history.assign ((size_t) nch * kTaps, 0.0f);
        histPos = 0;

        delayLen = totalDelay;
        delayBuf.assign ((size_t) nch * (size_t) delayLen, 0.0f);
        delayPos = 0;

        minVals.assign ((size_t) lookahead + 1, 1.0);
        minIdx.assign ((size_t) lookahead + 1, 0);
        dqHead = dqTail = 0; counter = 0;

        box.assign ((size_t) lookahead, 1.0);
        boxPos = 0; boxSum = (double) lookahead;

        env = 1.0;
        setRelease (0.080);
        reset();
    }

    void reset()
    {
        std::fill (history.begin(), history.end(), 0.0f);
        std::fill (delayBuf.begin(), delayBuf.end(), 0.0f);
        dqHead = dqTail = 0; counter = 0;
        std::fill (box.begin(), box.end(), 1.0);
        boxSum = (double) lookahead; boxPos = 0;
        env = 1.0; lastGain = 1.0;
    }

    // margem de 0.2 dB para cobrir o erro do detector
    void setCeilingDb (double db) { ceiling = dbToLin (db - 0.2); }
    void setRelease (double seconds) { relCoef = std::exp (-1.0 / (seconds * sampleRate)); }
    int getLatency() const { return totalDelay; }
    double getLastGain() const { return lastGain; }

    // Processa um quadro (todos os canais) no lugar.
    void processFrame (float* frame, bool enabled)
    {
        // 1) histórico para o detector
        for (int c = 0; c < nch; ++c)
            history[(size_t) c * kTaps + (size_t) histPos] = frame[c];
        histPos = (histPos + 1) % kTaps;

        // 2) pico estimado (amostra t-kDetectDelay e 7 pontos entre amostras), ligado entre canais
        double peak = 0.0;
        for (int c = 0; c < nch; ++c)
        {
            const float* h = &history[(size_t) c * kTaps];
            for (int k = 0; k < kOver; ++k)
            {
                double v = 0.0;
                for (int i = 0; i < kTaps; ++i)
                    v += coef[k][i] * h[(histPos + i) % kTaps]; // i=0 é a mais antiga
                peak = std::max (peak, std::abs (v));
            }
        }

        double required = (enabled && peak > ceiling) ? ceiling / peak : 1.0;

        // 3) mínimo deslizante sobre o lookahead (deque monotônico)
        const int cap = lookahead + 1;
        while (dqTail != dqHead && minVals[(size_t) ((dqTail - 1 + cap) % cap)] >= required)
            dqTail = (dqTail - 1 + cap) % cap;
        minVals[(size_t) dqTail] = required;
        minIdx[(size_t) dqTail] = counter;
        dqTail = (dqTail + 1) % cap;
        while (minIdx[(size_t) dqHead] <= counter - lookahead)
            dqHead = (dqHead + 1) % cap;
        const double m = minVals[(size_t) dqHead];
        ++counter;

        // 4) release
        env = std::min (m, 1.0 - relCoef * (1.0 - env));

        // 5) média móvel (garante que o ganho já esteja baixo quando o pico chega)
        boxSum -= box[(size_t) boxPos];
        box[(size_t) boxPos] = env;
        boxSum += env;
        boxPos = (boxPos + 1) % lookahead;
        const double g = std::min (1.0, boxSum / (double) lookahead);
        lastGain = g;

        // 6) atraso do áudio e aplicação do ganho
        for (int c = 0; c < nch; ++c)
        {
            float* d = &delayBuf[(size_t) c * (size_t) delayLen];
            const float delayed = d[delayPos];
            d[delayPos] = frame[c];
            float out = (float) (delayed * g);
            if (enabled) // proteção final contra qualquer sobra numérica
                out = std::clamp (out, (float) -ceiling * 1.0001f, (float) ceiling * 1.0001f);
            frame[c] = out;
        }
        delayPos = (delayPos + 1) % delayLen;
    }

private:
    double sampleRate = 48000.0, ceiling = 1.0, relCoef = 0.999, env = 1.0, lastGain = 1.0;
    int nch = 2, lookahead = 72, totalDelay = 77;
    double coef[kOver][kTaps] {};
    std::vector<float> history; int histPos = 0;
    std::vector<float> delayBuf; int delayLen = 1, delayPos = 0;
    std::vector<double> minVals; std::vector<long long> minIdx; int dqHead = 0, dqTail = 0; long long counter = 0;
    std::vector<double> box; int boxPos = 0; double boxSum = 0.0;
};

//==============================================================================
struct Settings
{
    double inputTrimDb   = 0.0;
    double targetLufs    = -14.0;
    double maxBoostDb    = 12.0;
    double maxCutDb      = 20.0;
    double gateLufs      = -45.0;
    double ceilingDbtp   = -1.0;
    int    mode          = 0;     // 0 = Voz, 1 = Programa
    bool   levelerOn     = true;
    bool   limiterOn     = true;
};

struct ModeTiming { double windowS, attackS, releaseS, fastReleaseS, fastToleranceDb; };

inline ModeTiming timingFor (int mode)
{
    // attack = quando precisa abaixar; release = quando precisa subir
    // fast* = controle rápido para sons curtos que passam do alvo
    if (mode == 0) return { 0.8, 0.6, 2.0, 0.4, 3.0 };   // Voz (microfones)
    return { 3.0, 1.0, 4.0, 0.8, 2.0 };                  // Programa (master, vídeos, comerciais)
}

//==============================================================================
class Leveler
{
public:
    void prepare (double fs, int numChannels, int maxBlock)
    {
        sampleRate = fs;
        nch = std::max (1, numChannels);
        frame.assign ((size_t) nch, 0.0f);
        (void) maxBlock;
        currentMode = -1;
        applyMode (settings.mode);
        inMeter.prepare (fs, nch, timingFor (settings.mode).windowS);
        inMeter.setGate (settings.gateLufs);
        outMeter.prepare (fs, nch, 1.0);
        fastEnvCoef = 1.0 - std::exp (-1.0 / (kFastWindowS * fs));
        fastAttackCoef = std::exp (-1.0 / (kFastAttackS * fs));
        outMeter.setGate (-70.0);
        limiter.prepare (fs, nch);
        limiter.setCeilingDb (settings.ceilingDbtp);
        reset();
    }

    void reset()
    {
        inMeter.reset(); outMeter.reset(); limiter.reset();
        gainDb = 0.0; targetGainDb = 0.0; gateOpen = false;
        fastEnv = 0.0; fastCutDb = 0.0; fastTargetDb = 0.0; fastCounter = 0;
    }

    void setSettings (const Settings& s)
    {
        settings = s;
        if (s.mode != currentMode) applyMode (s.mode);
        limiter.setCeilingDb (s.ceilingDbtp);
        inMeter.setGate (s.gateLufs);
    }

    int getLatency() const { return limiter.getLatency(); }

    void process (float* const* channels, int numChannels, int numSamples)
    {
        const int nc = std::min (numChannels, nch);
        const double trim = dbToLin (settings.inputTrimDb);
        double minLimGain = 1.0;

        for (int n = 0; n < numSamples; ++n)
        {
            for (int c = 0; c < nc; ++c) frame[(size_t) c] = (float) (channels[c][n] * trim);
            for (int c = nc; c < nch; ++c) frame[(size_t) c] = 0.0f;

            if (inMeter.pushFrame (frame.data(), nch))
                updateTarget();

            // ganho suave em dB (nivelador lento, por loudness)
            if (settings.levelerOn)
            {
                const double coef = targetGainDb < gainDb ? attackCoef : releaseCoef;
                gainDb = targetGainDb + coef * (gainDb - targetGainDb);
            }
            else
            {
                gainDb = 0.0;
            }

            // Controle rápido: se um som curto (batida de vinheta, grito, aplauso)
            // passar do alvo + tolerância, abaixa em poucos milissegundos e
            // devolve aos poucos. O nivelador lento sozinho demora para reagir.
            fastEnv += fastEnvCoef * (inMeter.getLastFrameEnergy() - fastEnv);
            if ((fastCounter++ & 15) == 0)
            {
                fastTargetDb = 0.0;
                if (settings.levelerOn && fastEnv > 1.0e-13)
                {
                    const double momentary = -0.691 + 10.0 * std::log10 (fastEnv);
                    const double over = momentary + gainDb - (settings.targetLufs + fastToleranceDb);
                    if (over > 0.0) fastTargetDb = -std::min (over, settings.maxCutDb + 12.0);
                }
            }
            {
                const double coef = fastTargetDb < fastCutDb ? fastAttackCoef : fastReleaseCoef;
                fastCutDb = fastTargetDb + coef * (fastCutDb - fastTargetDb);
            }

            const double totalDb = gainDb + fastCutDb;
            const float g = (float) dbToLin (totalDb);
            for (int c = 0; c < nch; ++c) frame[(size_t) c] *= g;

            limiter.processFrame (frame.data(), settings.limiterOn);
            minLimGain = std::min (minLimGain, limiter.getLastGain());

            outMeter.pushFrame (frame.data(), nch);

            for (int c = 0; c < nc; ++c) channels[c][n] = frame[(size_t) c];
        }

        meterIn.store ((float) inMeter.getLoudness());
        meterOut.store ((float) outMeter.getLoudness());
        meterGain.store ((float) (gainDb + fastCutDb));
        meterLimit.store ((float) linToDb (minLimGain));
        meterGate.store (gateOpen);
    }

    // Valores para a tela (lidos pela thread da interface).
    std::atomic<float> meterIn { -120.0f }, meterOut { -120.0f }, meterGain { 0.0f }, meterLimit { 0.0f };
    std::atomic<bool>  meterGate { false };

private:
    void applyMode (int mode)
    {
        currentMode = mode;
        const auto t = timingFor (mode);
        inMeter.setWindow (t.windowS);
        attackCoef  = std::exp (-1.0 / (t.attackS  * sampleRate));
        releaseCoef = std::exp (-1.0 / (t.releaseS * sampleRate));
        fastReleaseCoef = std::exp (-1.0 / (t.fastReleaseS * sampleRate));
        fastToleranceDb = t.fastToleranceDb;
    }

    void updateTarget()
    {
        // Gate: em silêncio/ruído o ganho fica parado (não sobe chiado).
        gateOpen = inMeter.getLastBlockLoudness() > settings.gateLufs;
        // Só reage com voz suficiente na janela (evita reagir a finais de frase).
        if (inMeter.fractionAboveGate() < 0.25) return;

        const double wanted = settings.targetLufs - inMeter.getLoudness();
        targetGainDb = std::clamp (wanted, -settings.maxCutDb, settings.maxBoostDb);
    }

    Settings settings;
    double sampleRate = 48000.0;
    int nch = 2, currentMode = -1;
    std::vector<float> frame;
    LoudnessMeter inMeter, outMeter;
    TruePeakLimiter limiter;
    double gainDb = 0.0, targetGainDb = 0.0, attackCoef = 0.0, releaseCoef = 0.0;
    bool gateOpen = false;

    static constexpr double kFastWindowS = 0.10;  // janela do detector rápido
    static constexpr double kFastAttackS = 0.005; // abaixa em ~5 ms
    double fastEnv = 0.0, fastEnvCoef = 0.0, fastAttackCoef = 0.0, fastReleaseCoef = 0.0;
    double fastCutDb = 0.0, fastTargetDb = 0.0, fastToleranceDb = 3.0;
    unsigned fastCounter = 0;
};

} // namespace nivelador
