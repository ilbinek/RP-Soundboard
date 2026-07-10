// src/inputfile.h
//----------------------------------
// RP Soundboard Source Code
// Copyright (c) 2015 Marius Graefe
// All rights reserved
// Contact: rp_soundboard@mgraefe.de
//----------------------------------

#pragma once

#include <stdint.h>
#include "SampleSource.h"

class SampleBuffer;

struct InputFileOptions
{
	enum channel_layout_e
	{
		MONO = 0,
		STEREO,
	};

	channel_layout_e outputChannelLayout;
	int outputSampleRate;

	InputFileOptions() :
		outputChannelLayout(STEREO),
		outputSampleRate(48000)
	{
	}

	inline int getNumChannels() const
	{
		switch (outputChannelLayout)
		{
		case MONO:
			return 1;
		case STEREO:
			return 2;
		default:
			return 0;
		}
	}
};


class InputFile : public SampleSource
{
  public:
	virtual ~InputFile() {};
	virtual int open(const char* filename, double startPosSeconds = 0.0, double playTimeSeconds = -1.0) = 0;
	virtual int close() = 0;
	virtual bool done() const = 0;
	/** Seek to an absolute position in the file (seconds from start of file). */
	virtual int seek(double seconds) = 0;
	virtual int64_t outputSamplesEstimation() const = 0;
	/** Playback position in seconds relative to the crop/play window start. */
	virtual double getPositionSeconds() const = 0;
	/** Playable duration in seconds (crop window if set). Returns 0 if unknown. */
	virtual double getDurationSeconds() const = 0;
	/** Absolute file position (seconds) corresponding to the start of the play window. */
	virtual double getStartPosSeconds() const = 0;
};

extern InputFile* CreateInputFileFFmpeg(InputFileOptions options = InputFileOptions());
