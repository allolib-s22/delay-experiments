/*	Gamma - Generic processing library
	See COPYRIGHT file for authors and license information

Example:	Echo
Author:		Lance Putnam, 2012

Description:
This demonstrates how to create an echo effect from a delay line. The output of
the delay line is fed back into the input to create a series of exponentially
decaying echoes. A low-pass "loop" filter is inserted into the feedback path to
simulate high-frequency damping due to air absorption.
*/

#include <cstdio> // for printing to stdout

#include "Gamma/Analysis.h"
#include "Gamma/Effects.h"
#include "Gamma/Envelope.h"
#include "Gamma/Oscillator.h"

#include "al/app/al_App.hpp"
#include "al/graphics/al_Shapes.hpp"
#include "al/scene/al_PolySynth.hpp"
#include "al/scene/al_SynthSequencer.hpp"
#include "al/ui/al_ControlGUI.hpp"
#include "al/ui/al_Parameter.hpp"

// using namespace gam;
using namespace al;

#include "al/app/al_App.hpp"

#include "Gamma/Delay.h"
#include "Gamma/Filter.h"
#include "Gamma/Oscillator.h"

using namespace gam;

class SineEnv : public SynthVoice
{
public:
	// Unit generators
	gam::Pan<> mPan;
	gam::Sine<> mOsc;
	gam::Env<3> mAmpEnv;
	// envelope follower to connect audio output to graphics
	gam::EnvFollow<> mEnvFollow;

	// Additional members
	Mesh mMesh;

	// Initialize voice. This function will only be called once per voice when
	// it is created. Voices will be reused if they are idle.
	void init() override
	{
		// Intialize envelope
		mAmpEnv.curve(0); // make segments lines
		mAmpEnv.levels(0, 1, 1, 0);
		mAmpEnv.sustainPoint(2); // Make point 2 sustain until a release is issued

		// We have the mesh be a sphere
		addDisc(mMesh, 1.0, 30);

		// This is a quick way to create parameters for the voice. Trigger
		// parameters are meant to be set only when the voice starts, i.e. they
		// are expected to be constant within a voice instance. (You can actually
		// change them while you are prototyping, but their changes will only be
		// stored and aplied when a note is triggered.)

		createInternalTriggerParameter("amplitude", 0.3, 0.0, 1.0);
		createInternalTriggerParameter("frequency", 60, 20, 5000);
		createInternalTriggerParameter("attackTime", 1.0, 0.01, 3.0);
		createInternalTriggerParameter("releaseTime", 3.0, 0.1, 10.0);
		createInternalTriggerParameter("pan", 0.0, -1.0, 1.0);
	}

	// The audio processing function
	void onProcess(AudioIOData &io) override
	{
		// Get the values from the parameters and apply them to the corresponding
		// unit generators. You could place these lines in the onTrigger() function,
		// but placing them here allows for realtime prototyping on a running
		// voice, rather than having to trigger a new voice to hear the changes.
		// Parameters will update values once per audio callback because they
		// are outside the sample processing loop.
		mOsc.freq(getInternalParameterValue("frequency"));
		mAmpEnv.lengths()[0] = getInternalParameterValue("attackTime");
		mAmpEnv.lengths()[2] = getInternalParameterValue("releaseTime");
		mPan.pos(getInternalParameterValue("pan"));
		while (io())
		{
			float s1 = mOsc() * mAmpEnv() * getInternalParameterValue("amplitude");
			float s2;
			mEnvFollow(s1);
			mPan(s1, s1, s2);
			io.out(0) += s1;
			io.out(1) += s2;
		}
		// We need to let the synth know that this voice is done
		// by calling the free(). This takes the voice out of the
		// rendering chain
		if (mAmpEnv.done() && (mEnvFollow.value() < 0.001f))
			free();
	}
};

class MyApp : public App
{
public:
	SynthGUIManager<SineEnv> synthManager{"SineEnv"};

	// Accum<> tmr;   // Timer for triggering sound
	// SineD<> src;   // Sine grain
	// Delay<> delay; // Delay line
	// OnePole<> lpf;

	MyApp()
	{
		// Allocate 200 ms in the delay line
		// delay.maxDelay(0.2);

		// tmr.period(4);
		// tmr.phaseMax();

		// // Configure a short cosine grain
		// src.set(1000, 0.8, 0.04, 0.25);

		// // Set up low-pass filter
		// lpf.type(gam::LOW_PASS);
		// lpf.freq(2000);
	}

	// void onAudio(AudioIOData &io)
	// {
	// 	while (io())
	// 	{

	// 		if (tmr())
	// 			src.reset();

	// 		float s = src();

	// 		// Read the end of the delay line to get the echo
	// 		float echo = delay();

	// 		// Low-pass filter and attenuate the echo
	// 		echo = lpf(echo) * 0.8;

	// 		// Write sum of current sample and echo to delay line
	// 		delay(s + echo);

	// 		// Finally output sum of dry and wet signals
	// 		s += echo;

	// 		io.out(0) = io.out(1) = s;
	// 	}
	// }

	void onCreate() override
	{
		navControl().active(false); // Disable navigation via keyboard, since we
									// will be using keyboard for note triggering

		// Set sampling rate for Gamma objects from app's audio
		gam::sampleRate(audioIO().framesPerSecond());

		imguiInit();

		// Play example sequence. Comment this line to start from scratch
		// synthManager.synthSequencer().playSequence("synth1.synthSequence");
		synthManager.synthRecorder().verbose(true);
	}

	// The audio callback function. Called when audio hardware requires data
	void onSound(AudioIOData &io) override
	{
		synthManager.render(io); // Render audio
	}

	void onAnimate(double dt) override
	{
		// The GUI is prepared here
		imguiBeginFrame();
		// Draw a window that contains the synth control panel
		synthManager.drawSynthControlPanel();
		imguiEndFrame();
	}

	// The graphics callback function.
	void onDraw(Graphics &g) override
	{
		g.clear();
		// Render the synth's graphics
		synthManager.render(g);

		// GUI is drawn here
		imguiDraw();
	}

	// Whenever a key is pressed, this function is called
	bool onKeyDown(Keyboard const &k) override
	{
		if (ParameterGUI::usingKeyboard())
		{ // Ignore keys if GUI is using
		  // keyboard
			return true;
		}
		if (k.shift())
		{
			// If shift pressed then keyboard sets preset
			int presetNumber = asciiToIndex(k.key());
			synthManager.recallPreset(presetNumber);
		}
		else
		{
			// Otherwise trigger note for polyphonic synth
			int midiNote = asciiToMIDI(k.key());
			if (midiNote > 0)
			{
				synthManager.voice()->setInternalParameterValue(
					"frequency", ::pow(2.f, (midiNote - 69.f) / 12.f) * 432.f);
				synthManager.triggerOn(midiNote);
			}
		}
		return true;
	}

	// Whenever a key is released this function is called
	bool onKeyUp(Keyboard const &k) override
	{
		int midiNote = asciiToMIDI(k.key());
		if (midiNote > 0)
		{
			synthManager.triggerOff(midiNote);
		}
		return true;
	}

	void onExit() override { imguiShutdown(); }
};


int main()
{

	MyApp app;

	// Set up audio
	app.configureAudio(48000., 512, 2, 0);

	app.start();
}
