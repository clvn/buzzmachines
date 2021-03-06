#include <windef.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <float.h>
#include <MachineInterface.h>
#include <dsplib.h>

#pragma optimize ("a", on)

#define MAX_TRACKS				8

#define EGS_ATTACK				0
#define EGS_SUSTAIN				1 
#define EGS_RELEASE				2
#define EGS_NONE				3

#define MIN_AMP					(0.0001 * (32768.0 / 0x7fffffff))

double const oolog2 = 1.0 / log(2);

CMachineParameter const paraAttack = 
{ 
	pt_word,										// type
	"Attack",
	"Attack time in ms",							// description
	1,												// MinValue	
	0xffff,											// MaxValue
	0,												// NoValue
	MPF_STATE,										// Flags
	16
};

CMachineParameter const paraSustain = 
{  
	pt_word,										// type
	"Sustain",
	"Sustain time in ms",							// description
	1,												// MinValue	
	0xffff,											// MaxValue
	0,												// NoValue
	MPF_STATE,										// Flags
	16
};

CMachineParameter const paraRelease = 
{ 
	pt_word,										// type
	"Release",
	"Release time in ms",							// description
	1,												// MinValue	
	0xffff,											// MaxValue
	0,												// NoValue
	MPF_STATE,										// Flags
	512
};

CMachineParameter const paraColor = 
{ 
	pt_word,										// type
	"Color",
	"Noise color (0=black, 1000=white)",			// description
	0,												// MinValue	
	0x1000,											// MaxValue
	0xffff,											// NoValue
	MPF_STATE,										// Flags
	0x1000
};

CMachineParameter const paraVolume = 
{ 
	pt_byte,										// type
	"Volume",
	"Volume [sustain level] (0=0%, 80=100%, FE=~200%)",	// description
	0,												// MinValue	
	0xfe,  											// MaxValue
	0xff,    										// NoValue
	MPF_STATE,										// Flags
	0x80
};

CMachineParameter const paraTrigger = 
{ 
	pt_switch,										// type
	"Trigger",
	"Trigger (1=on, 0=off)",						// description
	-1, 											// MinValue	
	-1,			  									// MaxValue
	SWITCH_NO,    									// NoValue
	0,												// Flags
	0
};


CMachineParameter const *pParameters[] = { 
	// track
	&paraAttack,
	&paraSustain,
	&paraRelease,
	&paraColor,
	&paraVolume,
	&paraTrigger
};

#pragma pack(1)

class tvals
{
public:
	word attack;
	word sustain;
	word release;
	word color;
	byte volume;
	byte trigger;

};

#pragma pack()

CMachineInfo const MacInfo = 
{
	MT_GENERATOR,							// type
	MI_VERSION,
	0,										// flags
	1,										// min tracks
	MAX_TRACKS,								// max tracks
	0,										// numGlobalParameters
	6,										// numTrackParameters
	pParameters,
	0, 
	NULL,
#ifdef _DEBUG
	"Jeskola Noise Generator (Debug build)",			// name
#else
	"Jeskola Noise Generator",
#endif
	"Noise",								// short name
	"Oskari Tammelin", 						// author
	NULL
};

class mi;

class mi;

class CTrack
{
public:
	void Tick(tvals const &tv);
	void Stop();
	void Reset();
	void Generate(float *psamples, int numsamples);
	void Noise(float *psamples, int numsamples);

	int MSToSamples(double const ms);

public:
	double Amp;
	double AmpStep;
	double S1;
	double S2;

	float Volume;
	int Pos;
	int Step;
	int RandStat;
	
	int EGStage;
	int EGCount;
	int Attack;
	int Sustain;
	int Release;

	mi *pmi;
};

class mi : public CMachineInterface
{
public:
	mi();
	virtual ~mi();

	virtual void Init(CMachineDataInput * const pi);
	virtual void Tick();
	virtual bool Work(float *psamples, int numsamples, int const mode);
	virtual void SetNumTracks(int const n) { numTracks = n; }
	virtual void Stop();

public:
	int numTracks;
	CTrack Tracks[MAX_TRACKS];

	tvals tval[MAX_TRACKS];

};

DLL_EXPORTS

mi::mi()
{
	GlobalVals = NULL;
	TrackVals = tval;
	AttrVals = NULL;
}

mi::~mi()
{

}

inline int CTrack::MSToSamples(double const ms)
{
	return (int)(pmi->pMasterInfo->SamplesPerSec * ms * (1.0 / 1000.0));
}

inline double CalcStep(double from, double to, int time)
{
	assert(from > 0);
	assert(to > 0);
	assert(time > 0);
	return pow(to / from, 1.0 / time);
}

void CTrack::Reset()
{
	EGStage = EGS_NONE;

	Attack = MSToSamples(16);
	Sustain = MSToSamples(16);
	Release = MSToSamples(512);

	Pos = 0;
	Step = 65536;
	Volume = 1.0;
	S1 = S2 = 0;
	RandStat = 0x16BA2118;
}

void mi::Init(CMachineDataInput * const pi)
{
#ifndef _MSC_VER
    DSP_Init(pMasterInfo->SamplesPerSec);
#endif
	for (int c = 0; c < MAX_TRACKS; c++)
	{
		Tracks[c].pmi = this;
		Tracks[c].Reset();
	}

}

void CTrack::Tick(tvals const &tv)
{
	if (tv.attack != paraAttack.NoValue)
		Attack = MSToSamples(tv.attack);

	if (tv.sustain != paraSustain.NoValue)
		Sustain = MSToSamples(tv.sustain);

	if (tv.release != paraRelease.NoValue)
		Release = MSToSamples(tv.release);

	if (tv.color != paraColor.NoValue)
		Step = tv.color * 16;	// 0..4096 -> 0..65536
	
	if (tv.volume != paraVolume.NoValue)
		Volume = (float)(tv.volume * (1.0 / 0x80));

	if (tv.trigger != SWITCH_NO)
	{
		if (Volume > 0)
		{
			EGStage = EGS_ATTACK;
			EGCount = Attack;
			Amp = MIN_AMP;
			AmpStep = CalcStep(MIN_AMP, Volume * (32768.0 / 0x7fffffff), Attack);
		}
	}
}

void CTrack::Noise(float *psamples, int numsamples)
{
	double amp = Amp;
	double const ampstep = AmpStep;		
	double s1 = S1;
	double s2 = S2;
	int const step = Step;
	int stat = RandStat;
	int pos = Pos;

	int c = numsamples;
	do
	{
		*psamples++ = (float)(s1 + (s2 - s1) * (pos * 1.0 / 65536.0));
		amp *= ampstep;

		pos += step;
		if (pos & 65536)
		{
			s1 = s2;
			stat = ((stat * 1103515245 + 12345) & 0x7fffffff) - 0x40000000;
			s2 = stat * amp;

			pos -= 65536;
		}

	} while(--c);

	Pos = pos;
	S2 = s2;
	S1 = s1;
	RandStat = stat;
	Amp = amp;
}

void CTrack::Generate(float *psamples, int numsamples)
{
	do
	{
		int const c = __min(EGCount, numsamples);
		assert(c > 0);

		if (EGStage != EGS_NONE)
			Noise(psamples, c);
		else
			memset(psamples, 0, c * sizeof(float));
		
		numsamples -= c;
		psamples += c;
		EGCount -= c;

		if (!EGCount)
		{
			switch(++EGStage)
			{
			case EGS_SUSTAIN:
				EGCount = Sustain;
				AmpStep = 1.0;
				break;
			case EGS_RELEASE:
				EGCount = Release;
				AmpStep = CalcStep(Amp, MIN_AMP, Release);
				break;
			case EGS_NONE:
				EGCount = 0x7fffffff;
				break;
			}
		}

		

	} while(numsamples > 0);
}
 
void mi::Tick()
{
	for (int c = 0; c < numTracks; c++)
		Tracks[c].Tick(tval[c]);

}

bool mi::Work(float *psamples, int numsamples, int const)
{
	bool gotsomething = false;

	for (int c = 0; c < numTracks; c++)
	{
		if (Tracks[c].EGStage != EGS_NONE)
		{
			if (!gotsomething)
			{
				Tracks[c].Generate(psamples, numsamples);
				gotsomething = true;
			}
			else
			{
				float *paux = pCB->GetAuxBuffer();
				Tracks[c].Generate(paux, numsamples);

				DSP_Add(psamples, paux, numsamples);

			}
		}
	}

	return gotsomething;
}

void CTrack::Stop()
{
	EGStage = EGS_NONE;
}

void mi::Stop()
{
	for (int c = 0; c < numTracks; c++)
		Tracks[c].Stop();
}
 
