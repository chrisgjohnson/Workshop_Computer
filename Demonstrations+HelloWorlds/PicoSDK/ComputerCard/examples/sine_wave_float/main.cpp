#include "ComputerCard.h"
#include <cmath>

/// Outputs sine wave at 440Hz

/// Uses floating-point numbers, which are relatively slow compared to integers.
/// Only about two such sine wave evaluations are possible per 48kHz sample.
/// See sine_wave_lookup example for a faster integer lookup table implementation.

/// Note the use of float literals (e.g. 440.0f) not double literals (e.g. 440.0)
/// and floating-point sine function 'sinf' (not 'sin'), since double calculations
/// are much slower than float on RP2040.

class SineWaveFloat : public ComputerCard
{
public:

	float phase;
	
	SineWaveFloat()
	{
		// Initialise phase of sine wave
		phase = 0.0f;
		
	}
	
	virtual void ProcessSample()
	{
		
		int32_t out = 2000 * sinf(phase);
		
		AudioOut1(out);
		AudioOut2(out);
		
		phase += M_TWOPI * 440.0f/48000.0f;
		if (phase > M_TWOPI) phase -= M_TWOPI;
	}
};


int main()
{
	SineWaveFloat sw;
	sw.Run();
}

  