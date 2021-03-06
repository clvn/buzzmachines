
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <MachineInterface.h>
#include "../dspchips/DSPChips.h"

#define MAX_TAPS		1
// 200 ms przy 44100 Hz

static int times[]={
  1,2,3,4,
  6,8,12,16,
  24,28,32,48,
  64,96,128
};

///////////////////////////////////////////////////////////////////////////////////

CMachineParameter const paraCutoff = 
{ 
	pt_byte,										// type
	"Cutoff",
	"Cutoff frequency",					// description
	0,												  // MinValue	
	240,												  // MaxValue
	255,										// NoValue
	MPF_STATE,										// Flags
	120
};

CMachineParameter const paraResonance = 
{ 
	pt_byte,										// type
	"Resonance",
	"Resonance",					// description
	0,												  // MinValue	
	240,												  // MaxValue
	255,										// NoValue
	MPF_STATE,										// Flags
	120
};

CMachineParameter const paraFilterType = 
{ 
	pt_byte,										// type
	"Filter",
	"Filter type",	// description
	0,												  // MinValue	
	7,												  // MaxValue
	255,										// NoValue
	MPF_STATE,										// Flags
	6
};

CMachineParameter const paraLFODepth = 
{ 
	pt_byte,									// type
	"LFO mod",
	"LFO Modulation depth",// description
	0,												// MinValue	
	240,											// MaxValue
	255,										  // NoValue
	MPF_STATE,								// Flags
	60,                       // default
};

CMachineParameter const paraLFORate = 
{ 
	pt_byte,										// type
	"LFO rate",
	"LFO rate [Hz]",				    // description
	0,												  // MinValue	
	254,												// MaxValue
	255,									// NoValue
	MPF_STATE,									// Flags
	30
};

CMachineParameter const paraLFOShape = 
{ 
	pt_byte,										// type
	"LFO Shape",
	"LFO Shape",	// description
	0,												  // MinValue	
	16,												  // MaxValue
	255,										// NoValue
	MPF_STATE,										// Flags
	0
};

CMachineParameter const paraInertia = 
{ 
	pt_byte,										// type
	"Inertia",
	"Cutoff frequency inertia",	// description
	0,												  // MinValue	
	60,												  // MaxValue
	255,										// NoValue
	MPF_STATE,										// Flags
	10
};

CMachineParameter const paraLFOPhase = 
{ 
	pt_byte,										// type
	"Set phase",
	"Set LFO phase",				    // description
	0,												  // MinValue	
	127,												// MaxValue
	255,									// NoValue
	MPF_STATE,									// Flags
	0
};



CMachineParameter const *pParameters[] = 
{ 
	&paraCutoff,
	&paraResonance,
  &paraFilterType,
	&paraLFORate,
	&paraLFODepth,
  &paraLFOShape,
	&paraInertia,
	&paraLFOPhase,
};

CMachineAttribute const attrMaxDelay = 
{
	"Theviderness factor",
	0,
	20,
	0
};

CMachineAttribute const *pAttributes[] = 
{
	&attrMaxDelay
};


#pragma pack(1)

class gvals
{
public:
	byte dryout;
};

class tvals
{
public:
  byte cutoff;
  byte resonance;
  byte filtype;
  byte lforate;
  byte lfodepth;
  byte lfoshape;
  byte inertia;
  byte lfophase;
};

class avals
{
public:
	int thevfactor;
};

#pragma pack()

CMachineInfo const MacInfo = 
{
	MT_EFFECT,								// type
	MI_VERSION,
	0,										// flags
	1,										// min tracks
	MAX_TAPS,								// max tracks
	0,										// numGlobalParameters
	8,										// numTrackParameters
	pParameters,
	1,                    // 1 (numAttributes)
	pAttributes,                 // pAttributes
#ifdef _DEBUG
	"FSM WahMan Pro 2 (Debug build)",			// name
#else
	"FSM WahMan Pro 2",
#endif
	"WahPRO",								// short name
	"Krzysztof Foltman",						// author
	"A&bout"
};

class CTrack
{
public:
/*
  int Length;
	int Pos;
	int Unit;
  */
  float Cutoff;
  float Resonance;
  float LFORate;
  float LFODepth;
  float Inertia;
  double LFOPhase;
  double DeltaPhase;

  float CurCutoff;

  CBiquad m_filter, m_filter2;
  int FilterType;
  int LFOShape;
  float ThevFactor;

  void CalcCoeffs1();
  void CalcCoeffs2();
  void CalcCoeffs3();
  void CalcCoeffs4();
  void CalcCoeffs5();
  void CalcCoeffs6();
  void CalcCoeffs7();
  void CalcCoeffs8();
  void ResetFilter() { m_filter.Reset(); m_filter2.Reset(); }
};
 

void CTrack::CalcCoeffs1()
{
	int sr=44100;

  float CutoffFreq=(float)(264*pow(32,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=20000) cf=20000; // pr�ba wprowadzenia nieliniowo�ci przy ko�cu charakterystyki
	if (cf<33) cf=(float)(33.0);
  float ScaleResonance=(float)pow(cf/20000.0,ThevFactor);
  // float ScaleResonance=1.0;
  float fQ=(float)(1.01+5*Resonance*ScaleResonance/240.0);

  float fB=(float)sqrt(fQ*fQ-1)/fQ;
	float fA=(float)(2*fB*(1-fB));

  float A,B;

	float ncf=(float)(1.0/tan(M_PI*cf/(double)sr));
	A=fA*ncf;      // denormalizacja i uwzgl�dnienie cz�stotliwo�ci pr�bkowania
	B=fB*ncf*ncf;
  float a0=float(1/(1+A+B));
	m_filter.m_b1=2*(m_filter.m_b2=m_filter.m_b0=a0/sqrt(fQ));// obliczenie wsp�czynnik�w filtru cyfrowego (przekszta�cenie dwuliniowe)
	m_filter.m_a1=a0*(2-B-B);
	m_filter.m_a2=a0*(1-A+B);
}

void CTrack::CalcCoeffs2()
{
  float CutoffFreq=(float)(264*pow(32,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=20000) cf=20000; // pr�ba wprowadzenia nieliniowo�ci przy ko�cu charakterystyki
	if (cf<33) cf=(float)(33.0);
  float ScaleResonance=(float)pow(cf/20000.0,ThevFactor);
  // float ScaleResonance=1.0;
  float fQ=(float)(1.01+30*Resonance*ScaleResonance/240.0);

  m_filter.SetParametricEQ(cf,10,fQ,44100,1/pow(fQ,0.5));
}

void CTrack::CalcCoeffs3()
{
  float CutoffFreq=(float)(264*pow(32,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=20000) cf=20000; // pr�ba wprowadzenia nieliniowo�ci przy ko�cu charakterystyki
	if (cf<33) cf=(float)(33.0);
  // float ScaleResonance=(float)pow(cf/20000.0,0.5);
  float Res=Resonance;
  Res*=(float)pow(cf/20000.0,ThevFactor);

  m_filter.SetParametricEQ(cf,float(1.0+Res/6.0),32,44100,0.2/(1+(240-Resonance)/180.0f));
}

void CTrack::CalcCoeffs4()
{
	int sr=44100;

  float CutoffFreq=(float)(264*pow(32,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=20000) cf=20000; // pr�ba wprowadzenia nieliniowo�ci przy ko�cu charakterystyki
	if (cf<33) cf=(float)(33.0);
  // float ScaleResonance=(float)pow(cf/20000.0,0.5);
  float ScaleResonance=(float)pow(cf/20000.0,ThevFactor);
  float fQ=(float)sqrt(1.01+5*Resonance*ScaleResonance/240.0);

  float fB=(float)sqrt(fQ*fQ-1)/fQ;
	float fA=(float)(2*fB*(1-fB));

  float A,B;

	float ncf=(float)(1.0/tan(M_PI*cf/(double)sr));
	A=fA*ncf;      // denormalizacja i uwzgl�dnienie cz�stotliwo�ci pr�bkowania
	B=fB*ncf*ncf;
  float a0=float(1/(1+A+B));
	m_filter.m_b1=2*(m_filter.m_b2=m_filter.m_b0=a0);// obliczenie wsp�czynnik�w filtru cyfrowego (przekszta�cenie dwuliniowe)
	m_filter.m_a1=a0*(2-B-B);
	m_filter.m_a2=a0*(1-A+B);

	ncf=(float)(1.0/tan(M_PI*(cf*0.7)/(double)sr));
	A=fA*ncf;      // denormalizacja i uwzgl�dnienie cz�stotliwo�ci pr�bkowania
	B=fB*ncf*ncf;
  a0=float(1/(1+A+B));
	m_filter2.m_b1=2*(m_filter2.m_b2=m_filter2.m_b0=0.35*a0);// obliczenie wsp�czynnik�w filtru cyfrowego (przekszta�cenie dwuliniowe)
	m_filter2.m_a1=a0*(2-B-B);
	m_filter2.m_a2=a0*(1-A+B);
}

void CTrack::CalcCoeffs5()
{
  float CutoffFreq=(float)(264*pow(32,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=20000) cf=20000; // pr�ba wprowadzenia nieliniowo�ci przy ko�cu charakterystyki
	if (cf<33) cf=(float)(33.0);
  // float ScaleResonance=(float)pow(cf/20000.0,0.5);
  float Res=Resonance*(float)pow(cf/20000.0,ThevFactor);

  m_filter.SetParametricEQ(cf,(float)(1.0+Res/12.0),float(6+Res/30.0),44100,0.4f/(1+(240-Res)/120.0f));
  m_filter2.SetParametricEQ(float(cf/(1+Res/240.0)),float(1.0+Res/12.0),float(6+Res/30.0),44100,0.4f);
}

void CTrack::CalcCoeffs6()
{
  float CutoffFreq=(float)(264*pow(32,CurCutoff/240.0));
	float cf=(float)CutoffFreq;
	if (cf>=20000) cf=20000; // pr�ba wprowadzenia nieliniowo�ci przy ko�cu charakterystyki
	if (cf<33) cf=(float)(33.0);
  // float ScaleResonance=(float)pow(cf/20000.0,0.5);
  float Res=Resonance*(float)pow(cf/20000.0,ThevFactor);

  m_filter.SetParametricEQ(cf,8.0f,9.0f,44100,0.5f);
  m_filter2.SetParametricEQ(float(cf/(3.5-2*Res/240.0)),8.0f,9.0f,44100,0.4f);
}

#define THREESEL(sel,a,b,c) ((sel)<120)?((a)+((b)-(a))*(sel)/120):((b)+((c)-(b))*((sel)-120)/120)

void CTrack::CalcCoeffs7()
{
  float CutoffFreq=CurCutoff;
  //if (CutoffFreq<0) CutoffFreq=0;
  //if (CutoffFreq>240) CutoffFreq=240;
  float Cutoff1=THREESEL(CutoffFreq,270,400,800);
  float Cutoff2=THREESEL(CutoffFreq,2140,800,1150);
  //float Amp1=THREESEL(CutoffFreq,9.0f,9.0f,9.0f);
  //float Amp2=THREESEL(CutoffFreq,9.0f,9.0f,9.0f);
  float ScaleResonance1=(float)pow(Cutoff1/20000.0,ThevFactor);
  float ScaleResonance2=(float)pow(Cutoff2/20000.0,ThevFactor);

  m_filter.SetParametricEQ(Cutoff1,2.0f+Resonance*ScaleResonance1/48.0f,6.0f+Resonance*ScaleResonance1/24.0f,44100,0.3f);
  m_filter2.SetParametricEQ(Cutoff2,2.0f+Resonance*ScaleResonance2/48.0f,6.0f+Resonance*ScaleResonance2/24.0f,44100,0.3f);
}

void CTrack::CalcCoeffs8()
{
  float CutoffFreq=CurCutoff;
  //if (CutoffFreq<0) CutoffFreq=0;
  //if (CutoffFreq>240) CutoffFreq=240;
  float Cutoff1=THREESEL(CutoffFreq,270,400,650);
  float Cutoff2=THREESEL(CutoffFreq,2140,1700,1080);
  //float Amp1=THREESEL(CutoffFreq,9.0f,9.0f,9.0f);
  //float Amp2=THREESEL(CutoffFreq,9.0f,9.0f,9.0f);

  float ScaleResonance1=(float)pow(Cutoff1/20000.0,ThevFactor);
  float ScaleResonance2=(float)pow(Cutoff2/20000.0,ThevFactor);

  m_filter.SetParametricEQ(Cutoff1,2.0f+Resonance*ScaleResonance1/56.0f,6.0f+Resonance*ScaleResonance1/16.0f,44100,0.3f);
  m_filter2.SetParametricEQ(Cutoff2,2.0f+Resonance*ScaleResonance2/56.0f,6.0f+Resonance*ScaleResonance2/16.0f,44100,0.3f);
}

class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();

	virtual void Init(CMachineDataInput * const pi);
	virtual void Tick();
	virtual bool Work(float *psamples, int numsamples, int const mode);

	virtual void SetNumTracks(int const n);

	virtual void AttributesChanged();

	virtual char const *DescribeValue(int const param, int const value);
  virtual void Command(int const i);

private:
	void InitTrack(int const i);
	void ResetTrack(int const i);

	void TickTrack(CTrack *pt, tvals *ptval);
	void WorkTrack(CTrack *pt, float *pin, float *pout, int numsamples, int const mode);

public:
  int Pos;
  float DryOut;
	avals aval;

private:
	int numTracks;
	CTrack Tracks[MAX_TAPS];

	gvals gval;
	tvals tval[MAX_TAPS];
  int Counter;
};

DLL_EXPORTS

mi::mi()
{
	GlobalVals = &gval;
	TrackVals = tval;
	AttrVals = (int *)&aval;
}

mi::~mi()
{
}

#define LFOPAR2TIME(value) (0.05*pow(800.0,value/255.0))

char const *mi::DescribeValue(int const param, int const value)
{
	static char txt[36];

	switch(param)
	{
/*  case 0:
  case 4:
    if (value)
      sprintf(txt, "%4.1f dB", (double)(value/10.0-24.0) );
    else
      sprintf(txt, "-inf dB");
		break;
  case 5:
    sprintf(txt, "%4.1f %%", (double)(value*100.0/64.0-100.0) );
    break;
	case 1:   // min/delta delay
  case 2:
		sprintf(txt, "%4.1f ms", (double)(value/10.0) );
		break;
    */
  case 2:
    if (value==0) strcpy(txt,"2pole LP");
    if (value==1) strcpy(txt,"2pole Peak I");
    if (value==2) strcpy(txt,"2pole Peak II");
    if (value==3) strcpy(txt,"4pole LP");
    if (value==4) strcpy(txt,"4pole Peak");
    if (value==5) strcpy(txt,"4pole Peak II");
    if (value==6) strcpy(txt,"Vocal I");
    if (value==7) strcpy(txt,"Vocal II");
    break;
  case 3:		// LFO rate
		if (value<240)
      sprintf(txt, "%5.3f Hz", (double)LFOPAR2TIME(value));
    else
      sprintf(txt, "%d ticks", times[value-240]);
		break;
  case 5: // LFO shape
    if (value==0) strcpy(txt,"sine");
    if (value==1) strcpy(txt,"saw up");
    if (value==2) strcpy(txt,"saw down");
    if (value==3) strcpy(txt,"square");
    if (value==4) strcpy(txt,"triangle");
    if (value==5) strcpy(txt,"weird 1");
    if (value==6) strcpy(txt,"weird 2");
    if (value==7) strcpy(txt,"weird 3");
    if (value==8) strcpy(txt,"weird 4");
    if (value==9) strcpy(txt,"steps up");
    if (value==10) strcpy(txt,"steps down");
    if (value==11) strcpy(txt,"upsaws up");
    if (value==12) strcpy(txt,"upsaws down");
    if (value==13) strcpy(txt,"dnsaws up");
    if (value==14) strcpy(txt,"dnsaws down");
    if (value==15) strcpy(txt,"S'n'H 1");
    if (value==16) strcpy(txt,"S'n'H 2");
    break;
	default:
		return NULL;
	}

	return txt;
}

void mi::TickTrack(CTrack *pt, tvals *ptval)
{
	if (ptval->lforate != paraLFORate.NoValue)
	{
		if (ptval->lforate<240)
      pt->DeltaPhase = (float)(2*M_PI*LFOPAR2TIME(ptval->lforate)/pMasterInfo->SamplesPerSec);
    else
      pt->DeltaPhase = (float)(2*M_PI*(float(pMasterInfo->TicksPerSec))/(times[ptval->lforate-240]*pMasterInfo->SamplesPerSec));
  }
	if (ptval->lfophase != paraLFOPhase.NoValue)
		pt->LFOPhase = (float)(2*M_PI*ptval->lfophase/128.0);
	if (ptval->lfodepth!= paraLFODepth.NoValue)
		pt->LFODepth = (float)(ptval->lfodepth);
	if (ptval->inertia!= paraInertia.NoValue)
		pt->Inertia = (float)(ptval->inertia/240.0);
	if (ptval->cutoff!= paraCutoff.NoValue)
		pt->Cutoff = ptval->cutoff;
	if (ptval->resonance!= paraResonance.NoValue)
		pt->Resonance = (float)(ptval->resonance);
  if (ptval->lfoshape!=paraLFOShape.NoValue)
    pt->LFOShape = ptval->lfoshape;
  if (ptval->filtype!=paraFilterType.NoValue)
  {
    if (pt->FilterType != ptval->filtype)
      pt->ResetFilter();
    pt->FilterType = ptval->filtype;
  }
 }



void mi::Init(CMachineDataInput * const pi)
{
	numTracks = 1;
  Counter=0;

	for (int c = 0; c < MAX_TAPS; c++)
	{
    tvals vals;
    vals.cutoff=paraCutoff.DefValue;
    vals.resonance=paraResonance.DefValue;
    vals.lforate=paraLFORate.DefValue;
    vals.lfodepth=paraLFODepth.DefValue;
    vals.lfophase=paraLFOPhase.DefValue;
    vals.lfoshape=paraLFOShape.DefValue;
    vals.inertia=paraInertia.DefValue;
    vals.filtype=paraFilterType.DefValue;
    TickTrack(&Tracks[c], &vals);
    Tracks[c].ResetFilter();
	}
}

void mi::AttributesChanged()
{
/*
	MaxDelay = (int)(pMasterInfo->SamplesPerSec * (aval.maxdelay / 1000.0));
	for (int c = 0; c < numTracks; c++)
		InitTrack(c);
    */
}


void mi::SetNumTracks(int const n)
{
	if (numTracks < n)
	{
		for (int c = numTracks; c < n; c++)
			InitTrack(c);
	}
	else if (n < numTracks)
	{
		for (int c = n; c < numTracks; c++)
			ResetTrack(c);
	
	}
	numTracks = n;

}


void mi::InitTrack(int const i)
{
	Tracks[i].LFOPhase = 0;
	Tracks[i].ResetFilter();
}

void mi::ResetTrack(int const i)
{
}


void mi::Tick()
{
	for (int c = 0; c < numTracks; c++)
		TickTrack(&Tracks[c], &tval[c]);
}

#pragma optimize ("a", on) 

#define INTERPOLATE(pos,start,end) ((start)+(pos)*((end)-(start)))

static void DoWork(float *pin, float *pout, mi *pmi, int c, CTrack *trk)
{
  trk->ThevFactor=float(pmi->aval.thevfactor/20.0);
  float ai=(float)(10*exp(-trk->Inertia*9.0));
  for (int i=0; i<c; i+=64) // pow(2,CurCutoff/48.0)
  {
    float LFO=0.0;
    float Phs=(float)fmod(trk->LFOPhase,(float)(2*PI));
    switch(trk->LFOShape)
    {
      case 0: LFO=(float)sin(Phs); break;
      case 1: LFO=(float)(((Phs-PI)/PI-0.5f)*2.0f); break;
      case 2: LFO=(float)(((Phs-PI)/PI-0.5f)*-2.0f); break;
      case 3: LFO=(Phs<PI)?1.0f:-1.0f; break;
      case 4: LFO=(float)(((Phs<PI?(Phs/PI):(2.0-Phs/PI))-0.5)*2); break;
      case 5: LFO=(float)sin(trk->LFOPhase+PI/4*sin(trk->LFOPhase)); break;
      case 6: LFO=(float)sin(trk->LFOPhase+PI/6*sin(2*trk->LFOPhase)); break;
      case 7: LFO=(float)sin(2*trk->LFOPhase+PI*cos(3*trk->LFOPhase)); break;
      case 8: LFO=(float)(0.5*sin(2*trk->LFOPhase)+0.5*cos(3*trk->LFOPhase)); break;
      case 9: LFO=(float)(0.25*floor(((Phs-PI)/PI-0.5f)*2.0f*4.0)); break;
      case 10: LFO=(float)(-0.25*floor(((Phs-PI)/PI-0.5f)*2.0f*4.0)); break;
      case 11: LFO=(float)(0.125*floor(((Phs-PI)/PI-0.5f)*2.0f*4.0)+0.5*fmod(Phs,PI/4)/(PI/4)); break;
      case 12: LFO=(float)(-0.125*floor(((Phs-PI)/PI-0.5f)*2.0f*4.0)+0.5*fmod(Phs,PI/4)/(PI/4)); break;
      case 13: LFO=(float)(0.125*floor(((Phs-PI)/PI-0.5f)*2.0f*4.0)+0.5*fmod(2*PI-Phs,PI/4)/(PI/4)); break;
      case 14: LFO=(float)(-0.125*floor(((Phs-PI)/PI-0.5f)*2.0f*4.0)+0.5*fmod(2*PI-Phs,PI/4)/(PI/4)); break;
      case 15: LFO=(float)(0.5*(sin(19123*floor(trk->LFOPhase*8/PI)+40*sin(12*floor(trk->LFOPhase*8/PI))))); break; // 8 zmian/takt
      case 16: LFO=(float)(0.5*(sin(1239543*floor(trk->LFOPhase*4/PI)+40*sin(15*floor(trk->LFOPhase*16/PI))))); break; // 8 zmian/takt
    }
    float DestCutoff=(float)(trk->Cutoff+trk->LFODepth*LFO/2); // pow(2.0,trk->LFODepth*sin(trk->LFOPhase)/100.0);
    if (fabs(trk->CurCutoff-DestCutoff)<ai)
      trk->CurCutoff=DestCutoff;
    else
      trk->CurCutoff+=(float)_copysign(ai,DestCutoff-trk->CurCutoff);
    if (trk->FilterType==0) trk->CalcCoeffs1();
    if (trk->FilterType==1) trk->CalcCoeffs2();
    if (trk->FilterType==2) trk->CalcCoeffs3();
    if (trk->FilterType==3) trk->CalcCoeffs4();
    if (trk->FilterType==4) trk->CalcCoeffs5();
    if (trk->FilterType==5) trk->CalcCoeffs6();
    if (trk->FilterType==6) trk->CalcCoeffs7();
    if (trk->FilterType==7) trk->CalcCoeffs8();

    int jmax=__min(i+64,c);
    if (trk->FilterType<3)
    {
      if (pout)
      {
        for (int j=i; j<jmax; j++)
        {
          pout[j]=2.0f*trk->m_filter.ProcessSample(pin[j]);
        }
      }
      else
        for (int j=i; j<jmax; j++)
        {
          trk->m_filter.ProcessSample(pin[j]);
        }
    }
    else
    {
      if (pout)
      {
        for (int j=i; j<jmax; j++)
        {
          pout[j]=2.0f*trk->m_filter2.ProcessSample(trk->m_filter.ProcessSample(pin[j]));
        }
      }
      else
      {
        for (int j=i; j<jmax; j++)
          trk->m_filter2.ProcessSample(trk->m_filter.ProcessSample(pin[j]));
      }
    }
    trk->LFOPhase+=(jmax-i)*trk->DeltaPhase;
    if (trk->LFOPhase>1024*PI)
      trk->LFOPhase-=1024*PI;
  }
}

/*
static void DoWorkNoInput(float *pout, CMachineInterface *mi, int c, CTrack *trk)
{
	do
	{
		double delay = *pbuf;
		*pbuf++ = (float)(feedback * delay);
		*pout++ += (float)(delay * wetout);

	} while(--c);
}

static void DoWorkNoInputNoOutput(CMachineInterface *mi, int c, CTrack *trk)
{
	do
	{
		*pbuf = (float)(*pbuf * feedback);
		pbuf++;
	} while(--c);
}

static void DoWorkNoOutput(float *pin, CMachineInterface *mi, int c, CTrack *trk)
{
	do
	{

		double delay = *pbuf;
		double dry = *pin++;
		*pbuf++ = (float)(feedback * delay + dry);

	} while(--c);
}
*/

#pragma optimize ("", on)


void mi::WorkTrack(CTrack *pt, float *pin, float *pout, int numsamples, int const mode)
{
  DoWork(pin,pout,this,numsamples,pt);
}

bool mi::Work(float *psamples, int numsamples, int const mode)
{
	float *paux = pCB->GetAuxBuffer();

	if (mode & WM_READ)
  {
    Counter=0;
		memcpy(paux, psamples, numsamples*4);
  }
  else
  {
    CTrack *trk=Tracks;
    if (Counter>1000 && (trk->FilterType<2 || (fabs(trk->m_filter2.m_y1)<1 && fabs(trk->m_filter2.m_y2)<1)) && fabs(trk->m_filter.m_y2)<1 && fabs(trk->m_filter.m_y1)<1)
    {
      trk->LFOPhase+=numsamples*trk->DeltaPhase;
      return false;
    }
    Counter+=numsamples;
    for (int i=0; i<numsamples; i++)
      paux[i]=1e-15;
  }

  for (int c = 0; c < numTracks; c++)
    WorkTrack(Tracks + c, paux, (mode&WM_WRITE)?psamples:NULL, numsamples, mode);

  return true;
}

void mi::Command(int const i)
{
  pCB->MessageBox("FSM WahManPro version 2.2 (for XionD)\nWritten by Krzysztof Foltman (kf@cw.pl), Gfx by Oomek");
}

