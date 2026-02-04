#include "ComputerCard.h"
#include "pico/multicore.h"
#include "tusb.h"


/*
  Fairly minimal web interface demo using WebMIDI and SysEx
  to transfer data both ways between Workshop System and the browser.


  Both the Javascript in interface.html and C++ firmware in main.cpp
  provide two functions:
  SendSysEx
  ProcessIncomingSysEx
  which take an array of bytes (values 0 to 127) and provide bidirectional
  communication between the web interface and WS Computer firmware.

  On in both the Javascript and C++ sides, developers should fill out
  out an implementation of ProcessIncomingSysex to handle the incoming
  messages, and call SendSysEx to send messages.


  When SendSysEx is called on the WS Computer, the ProcessIncomingSysEx 
  function is called on the web interface, with the same data.

  And vice versa, if SendSysEx is called by the web interface,
  ProcessIncomingSysex is then called on the WS computer, with this data.


  Here a very simple protocol is used where the first byte of the message
  is used to indicate what type of message it is (not really necessary
  here as there is only one type of message going in each direction).
  All sorts of alternatives are possible, including sending 
  (7-bit ascii) text such as JSON.


  In a real card, it would also be sensible to have messages for checking
  that the card is correct and version number is consistent between
  HTML interface and firmware.

*/


// Class to add MIDI SysEx connectivity to ComputerCard

// This is not a carefully written generic library, but abstracts out the
// MIDI/SysEx handling that may not need to change much, if at all, between different cards
class WebInterfaceComputerCard : public ComputerCard
{
public:
	WebInterfaceComputerCard()
	{
		// Start the second core
		multicore_launch_core1(core1);
	}

	// Boilerplate static function to call member function as second core
	static void core1()
	{
		((WebInterfaceComputerCard *)ThisPtr())->USBCore();
	}

	void SendSysEx(uint8_t *data, uint32_t size)
	{
		uint8_t header[] = {0xF0, 0x7D};
		uint8_t footer[] = {0xF7};
		tud_midi_stream_write(0, header, 2);
		tud_midi_stream_write(0, data, size);
		tud_midi_stream_write(0, footer, 1);
	}

	// Code for second RP2040 core. Blocking.
	// Listens for SysEx messages over MIDI.
	void USBCore()
	{
		uint8_t buffer[256]; // max size of message

		// Initialise TinyUSB
		tusb_init();


		// This loop waits for and processes MIDI messages
		while (1)
		{
			tud_task();

			////////////////////////////////////////
			// Receiving MIDI
			while (tud_midi_available())
			{
				// Read MIDI input - this will be some number of MIDI messages (zero, one, or multiple)
				uint32_t bytesToProcess = tud_midi_stream_read(buffer, sizeof(buffer));
				uint8_t *bufPtr = buffer;

				while (bytesToProcess > 0)
				{
					if (bufPtr[0] == 0xF0) // start of SysEx message
					{
						// Find end of SysEx message
						uint8_t *msgEndPtr = bufPtr;
						uint32_t i = bytesToProcess;
						do 
						{
							msgEndPtr++;
							i--;
						} while (i > 0 && (!(*msgEndPtr == 0xF7)));

						if (i > 0)
						{
							ProcessIncomingSysEx(bufPtr + 2, msgEndPtr - bufPtr - 2);
						}
					}

					// To receive non-sysex MIDI messages, insert code here
					// (see e.g. midi_device example)
					
						
					// Move pointer to next midi message in buffer, if there is one
					// by scanning until we see a MIDI command byte (most significant bit set)
					do 
					{
						bufPtr++;
						bytesToProcess--;
					} while (bytesToProcess > 0 && (!(*bufPtr & 0x80)));			
				}
			}
			MIDICore();

		}
	}

	// New virtual function, overridden in specific class
	virtual void MIDICore() {}
	
	// New virtual function, overridden in specific class
	virtual	void ProcessIncomingSysEx(uint8_t */*data*/, uint32_t /*size*/)
	{
	}
};


// This specific demo
class WebInterfaceDemo : public WebInterfaceComputerCard
{
	// MIDICore is called continuously from the non-audio core.
	// It's a good place to send any SysEx back to the web interface.
	void MIDICore()
	{
		int32_t mainKnob = KnobVal(Main);
		if (lastMainKnob != mainKnob)
		{
			lastMainKnob = mainKnob;
			uint8_t vals[] = {0x01, uint8_t(mainKnob >> 5), uint8_t(mainKnob & 0x1F)};
			SendSysEx(vals, 3);
		}
	}

	// Called whenever a message is received from the web interface.
	void ProcessIncomingSysEx(uint8_t *data, uint32_t size)
	{
		// Two byte message from interface, starting with 0x02 = position from slider
		if (size == 2 && data[0] == 0x02)
		{
			sliderVal = data[1];
		}
	}

	
	// 48kHz audio processing function; runs on audio core
	virtual void ProcessSample()
	{
		// No audio I/O, so just flash an LED
		// to indicate that the card is running
		static int32_t frame=0;
		LedOn(5,(frame>>13)&1);
		frame++;


		// Set all CV and audio outs, and top 4 LEDs,
		// to value received from web interface slider
		CVOut1(sliderVal << 4);
		CVOut2(sliderVal << 4);
		AudioOut1(sliderVal << 4);
		AudioOut2(sliderVal << 4);
		LedBrightness(0, sliderVal << 4);
		LedBrightness(1, sliderVal << 4);
		LedBrightness(2, sliderVal << 4);
		LedBrightness(3, sliderVal << 4);
	}

private:
	// Volatile variable to communicate between the MIDI and audio cores
	volatile uint32_t sliderVal = 0;

	int32_t lastMainKnob = 0;
};


int main()
{
	WebInterfaceDemo wid;
	wid.Run();
}

  
