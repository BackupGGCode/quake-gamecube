/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Peter Mackay

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <SDL_audio.h>

extern "C"
{
#include "../generic/quakedef.h"
}

namespace quake
{
	namespace sound
	{
		// The type of sample Quake mixes.
		struct sample
		{
			Sint16	left;
			Sint16	right;
		};
		
		// The buffer SDL uses.
		static SDL_AudioCVT	conversion;
		static const int	samples_per_submission	= 1024;

		// Quake writes its audio into this mix buffer.
		static const int	samples_per_mix_buffer	= samples_per_submission * 4;
		static sample		mix_buffer[samples_per_mix_buffer];
		static int			mix_buffer_pointer		= 0;

		// Convert and copy.
		static void convert_and_copy(Uint8* src, int src_byte_count, Uint8* dst, int dst_byte_count)
		{
			// Is there room to convert inside the mix buffer?
			if (conversion.len_ratio <= 1.0)
			{
				// Do the conversion in the mix buffer.
				conversion.buf	= src;
				conversion.len	= src_byte_count;
				SDL_ConvertAudio(&conversion);

				// Copy to the stream.
				memcpy(dst, src, conversion.len_cvt);
			}
			else
			{
				// Copy to the stream.
				memcpy(dst, src, src_byte_count);

				// Do the conversion in the stream buffer.
				conversion.buf	= dst;
				conversion.len	= src_byte_count;
				SDL_ConvertAudio(&conversion);
			}
		}

		// The audio submission callback.
		static void callback(void*, Uint8 *dst, int dst_byte_count)
		{
			// Set up the source data.
			Uint8* const	src				= reinterpret_cast<Uint8*>(&mix_buffer[mix_buffer_pointer]);
			const int		src_byte_count	= sizeof(sample) * samples_per_submission;

			// Is audio conversion required?
			if (conversion.needed)
			{
				// Convert and copy at the same time.
				convert_and_copy(src, src_byte_count, dst, dst_byte_count);
			}
			else
			{
				// Copy directly to the stream.
				memcpy(dst, src, dst_byte_count);
			}

			// Advance the mix pointer.
			mix_buffer_pointer = (mix_buffer_pointer + samples_per_submission) % samples_per_mix_buffer;
		}
	}
}

using namespace quake;
using namespace quake::sound;

qboolean SNDDMA_Init(void)
{
	// Set up the format we want.
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.callback	= callback;
	desired.channels	= 2;
	desired.format		= AUDIO_S16SYS;
	desired.freq		= 22050;
	desired.padding		= 0;
	desired.samples		= samples_per_submission;

	// Set up Quake's audio.
	shm = &sn;
	shm->channels			= desired.channels;
	shm->samplebits			= (sizeof(sample) / shm->channels) * 8;
	shm->speed				= desired.freq;
	shm->soundalive			= qtrue;
	shm->splitbuffer		= qfalse;
	shm->samples			= samples_per_mix_buffer * shm->channels;
	shm->samplepos			= 0;
	shm->submission_chunk	= 1;
	shm->buffer				= reinterpret_cast<unsigned char*>(&mix_buffer[0]);

	// Initialise the audio system.
	SDL_AudioSpec obtained;
	if (SDL_OpenAudio(&desired, &obtained) < 0)
	{
		Sys_Error("SDL_OpenAudio failed");
		return qfalse;
	}

	// Set up the conversion info.
	if (SDL_BuildAudioCVT(
		&conversion,
		desired.format,
		shm->channels,
		shm->speed,
		obtained.format,
		obtained.channels,
		obtained.freq) < 0)
	{
		Sys_Error("SDL_BuildAudioCVT failed");
		return qfalse;
	}

	// Start the first chunk of audio playing.
	SDL_PauseAudio(0);

	return qtrue;
}

void SNDDMA_Shutdown(void)
{
	// Stop streaming.
	SDL_PauseAudio(0);

	// Stop the audio system.
	SDL_CloseAudio();
}

int SNDDMA_GetDMAPos(void)
{
	return mix_buffer_pointer * shm->channels;
}

void SNDDMA_Submit(void)
{
}
