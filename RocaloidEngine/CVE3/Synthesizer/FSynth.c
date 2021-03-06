#include "FSynth.h"
#include "../CVEDSP/DFT/FFT.h"
#include "../CVEDSP/Algorithm/FECSOLA.h"
#include "../CVEDSP/Algorithm/Formant.h"
#include "../CVEDSP/Interpolation.h"
#include "../CVEDSP/Plot.h"
#include "../CVEGlobal.h"
#include "../Debug/ALblLog.h"

#include "../DSPEx/FECSOLAEx.h"
#include "../DSPEx/LCFECSOLA.h"

_Constructor_ (FSynth)
{
    CSynth_Ctor(& Dest -> SubSynth);
    Dest -> SynthFreq = 500;
}

_Destructor_ (FSynth)
{
    CSynth_Dtor(& Dest -> SubSynth);
}

void FSynth_SetSymbol(FSynth* Dest, String* Symbol)
{
    ALblLog_Print("FSynth_SetSymbol %s", String_GetChars(Symbol));
    CSynth_SetSymbol(& Dest -> SubSynth, Symbol);
    Dest -> SynthFreq = Dest -> SubSynth.Data.Header.F0;
}

void FSynth_SetConsonantRatio(FSynth* Dest, float CRatio)
{
    CSynth_SetConsonantRatio(& Dest -> SubSynth, CRatio);
}

void FSynth_SetVowelRatio(FSynth* Dest, float VRatio)
{
    CSynth_SetVowelRatio(& Dest -> SubSynth, VRatio);
}

void FSynth_Reset(FSynth* Dest)
{
    CSynth_Reset(& Dest -> SubSynth);
    Dest -> SynthFreq = Dest -> SubSynth.Data.Header.F0;
}

void FSynth_SetFrequency(FSynth* Dest, float Freq)
{
    Dest -> SynthFreq = Freq;
}

float TmpF[1024];
void FSynth_Resample(PSOLAFrame* Dest, PSOLAFrame* Src, float Ratio)
{
    int i;
    int LOffset;
    float Offset;
    float IRatio;
    int SrcHalf = Src -> Length / 2;
    float* Center = Src -> Data + SrcHalf;

    Offset = (float)(0 - Dest -> Length / 2) * Ratio;
    if(Offset > - SrcHalf + 1)
    {
        //No bound checking
        for(i = 0; i < Dest -> Length; i ++)
        {
            LOffset = (int)(Offset + SrcHalf) - SrcHalf;
            IRatio = Offset - LOffset;
            Dest -> Data[i] = Center[LOffset] * (1 - IRatio) + Center[LOffset + 1] * IRatio;
            Offset += Ratio;
        }/*
        Boost_FloatMul(TmpF, Dest -> Data, 20, 1024);
        Boost_FloatAdd(TmpF, TmpF, 3.5, 1024);
        GNUPlot_SetTitleAndNumber("", Ratio * 100);
        GNUPlot_PlotFloat(TmpF, 1024);
        WaitForDraw(80000);*/
    }else
    {
        //Bound checking
        for(i = 0; i < Dest -> Length; i ++)
        {
            if(Offset > - SrcHalf + 1 && Offset < SrcHalf - 1)
            {
                //In Src range: linear interpolation.
                LOffset = (int)(Offset + SrcHalf) - SrcHalf;
                IRatio = Offset - LOffset;
                Dest -> Data[i] = Center[LOffset] * (1 - IRatio) + Center[LOffset + 1] * IRatio;
            }
            else
            {
                //Beyond Src range.
                Dest -> Data[i] = 0;
            }
            Offset += Ratio;
        }
    }
}

void QuadHPF(float* Dest, float Freq)
{
    int i;
    float UFreq = FreqToIndex1024(Freq);
    for(i = 0; i < FreqToIndex1024(Freq); i ++)
        Dest[i] *= i * i / UFreq / UFreq;
}

FSynthSendback FSynth_Synthesis(FSynth* Dest, FDFrame* Output)
{
    FSynthSendback Ret;
    PSOLAFrame BFWave;
    PSOLAFrame_CtorSize(& BFWave, 2048);
    PSOLAFrame TempWave;
    PSOLAFrame_CtorSize(& TempWave, 1024);

    float BF = Dest -> SubSynth.Data.Header.F0;
    CSynth_SetVowelRatio(& Dest -> SubSynth, BF / Dest -> SynthFreq);
    CSynthSendback SubRet = CSynth_Synthesis(& Dest -> SubSynth, & BFWave);
    Ret.BeforeVOT = SubRet.BeforeVOT;

    CPF_Setup(OrigEnv);
    float* OrigRe = FloatMalloc(2048);
    float* OrigIm = FloatMalloc(2048);
    float* OrigMa = FloatMalloc(2048);
    float* Orig   = FloatMalloc(2048);

    Boost_FloatMulArr(Orig, BFWave.Data, Hanning2048, 2048);

    RFFT(OrigRe, OrigIm, Orig, 11);

    //Nyquist LPF

    if(! Ret.BeforeVOT && BF / Dest -> SynthFreq < 1.0f)
    {
        int LPF = FreqToIndex2048(22050 * BF / Dest -> SynthFreq);
        Boost_FloatSet(OrigRe + LPF, 0, 1024 - LPF);
        Boost_FloatSet(OrigIm + LPF, 0, 1024 - LPF);
        Reflect(OrigRe, OrigIm, OrigRe, OrigIm, 11);
        RIFFT(Orig, OrigRe, OrigIm, 11);
    }

    Boost_FloatDivArr(BFWave.Data, Orig, Hanning2048, 2048);

    if(Ret.BeforeVOT)
    {
        //Consonants
        Boost_FloatCopy(TempWave.Data, BFWave.Data + 512, 1024);
        Boost_FloatMulArr(TempWave.Data, TempWave.Data, Hanning1024, 1024);
        FDFrame_FromPSOLAFrame(Output, & TempWave);

        //TODO: GetVOT from CDT
        float FreqExpand = 1;
        int TransLast = CSynth_GetVOT(& Dest -> SubSynth) - Dest -> SubSynth.PlayPosition + SubRet.PSOLAFrameHopSize;
        if(TransLast < FSynth_ConsonantTransition)
            FreqExpand = TransLast / FSynth_ConsonantTransition
                       + BF / Dest -> SynthFreq * (1.0f - TransLast / FSynth_ConsonantTransition);
        Ret.PSOLAFrameHopSize = SubRet.PSOLAFrameHopSize * FreqExpand;
    }else
    {
        //Boost_FloatCopy(TempWave.Data, BFWave.Data + 512, 1024);
        FSynth_Resample(& TempWave, & BFWave, Dest -> SynthFreq / BF);
        Ret.PSOLAFrameHopSize = SubRet.PSOLAFrameHopSize * BF / Dest -> SynthFreq;
        //Maintain Spectral Envelope
        #include "FSynthSpectrumModification.h"
    }

    CPF_Dtor(& OrigEnv);
    free(Orig); free(OrigRe); free(OrigIm); free(OrigMa);

    if(Ret.PSOLAFrameHopSize > 512)
        Ret.PSOLAFrameHopSize = 512;

    PSOLAFrame_Dtor(& BFWave);
    PSOLAFrame_Dtor(& TempWave);
    return Ret;
}

float FSynth_GetVOT(FSynth* Dest)
{
    return CSynth_GetVOT(& Dest -> SubSynth);
}
