/*
Name:				Diarmaid Farrell
Student number:		11531677
supervisor:			Alistair Sutherland
date:				23/05/2016

Plagairism statement:

I declare that this material, which I now submit for assessment, is entirely my
own work and has not been taken from the work of others, save and to the extent that such
work has been cited and acknowledged within the text of my work. I understand
that plagiarism, collusion, and copying are grave and serious offences in the university and
accept the penalties that would be imposed should I engage in plagiarism, collusion or
copying. I have read and understood the Assignment Regulations. I have identified
and included the source of all facts, ideas, opinions, and viewpoints of others in the
assignment references. Direct quotations from books, journal articles, internet sources,
module text, or any other source whatsoever are acknowledged and the source cited are
identified in the assignment references. This assignment, or any part of it, has not been
previously submitted by me/us or any other person for assessment on this or any other
course of study.

*/
#ifndef UNICODE
#define UNICODE
#define IDC_MAIN_BUTTON 101
#define IDC_2ND_BUTTON  103
#define IDC_3RD_BUTTON  104
#define IDC_4TH_BUTTON  105
#define IDC_5TH_BUTTON  106
#define IDC_6TH_BUTTON  107
#define IDC_7TH_BUTTON  108
#define IDC_8TH_BUTTON  109
#define IDC_9TH_BUTTON  110
#define IDC_10TH_BUTTON 111
#define IDC_11TH_BUTTON 112
#define IDC_12TH_BUTTON 114
#define IDC_MAIN_EDIT   102
#define IDC_MAIN_EDIT_2 113
#endif 
#include <stdio.h>
#include <math.h>
#include <portaudio.h>
#include <iostream>
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <iostream>       // std::cin, std::cout
#include <deque>          // std::queue
#include <queue>          // std::queue
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <complex>
#include <stdlib.h> //for finding memory leaks
#include <crtdbg.h> //this too
#define CRTDBG_MAP_ALLOC //this too, another line of code in the program exit to print memory leaks in output

using namespace Gdiplus;
#pragma comment (lib,"Gdiplus.lib")
HWND hEdit;
#define SAMPLE_RATE         (44100)
#define PA_SAMPLE_TYPE      paFloat32
#define FRAMES_PER_BUFFER   (64)

bool error;
bool pedalactive;//called pedalactive but should be called pedalactive 
bool fuzzEngaged = false; //will use this to allow DISTORT during looper and delay
bool hifilter = false; //change this to true for a high pass filter

bool looping = false;//all these booleans are for the looper -start
bool loopdeleting = false;
bool recording = false;
bool firstrecording = true;
bool loopeng = false;
bool overdub = false;//all these booleans are for the looper -end
int overdub_length = 0;

PaStreamParameters inputParameters, outputParameters;
PaStream *stream;
PaError err;

typedef float SAMPLE; //add a special float type called SAMPLE

float left_phase = 0; //the left and right phase for the tremolo
float right_phase = 0;

float chorus_cutoff = 200;
int bitmapchange = 0;//increment this until it hits 6300 then randomise the carrier signal to a number between 1 and -1
float bitmappitch = 0.0f; //the left and right pitch for the bitmap
float bitmappitchr = 0.0f;

float chorus_cutoff_low = 200; //band filter low and high
float chorus_cutoff_high = 100;
bool goingup;

//the delay needs to be 500ms  long to be noticable
std::deque<SAMPLE> delayqueue;
std::deque<SAMPLE> chorusqueue;
std::queue<SAMPLE> looperqueue; //queue for looper, max 10 seconds for performance reasons, ie size 441000
std::queue<SAMPLE> blankqueue;

//********************************************* ALL my effect callback functions start here *************************************************

class c_sinewave{
	public:
		static const double two_pi;
	private:
	const double amplitude;
		const unsigned int frequency;

	public:

	c_sinewave(const double a = 1.0, const unsigned int f = 1) : amplitude(a), frequency(f)
	{
	}

	const double value(const double x){
		if (x < 0 || x > 1){
			return 0;
		}
		else{
			return amplitude * ::sin((two_pi * frequency) * x);
		}
	}
};
const double c_sinewave::two_pi = ::atan(1) * 8.0;
//sine wave test below
/*
int main(int argc, char* argv[])
{
	// Test it.
	c_sinewave sinewave(5.0, 2); //what is 5.0 and 2? amplitude and frequency?

	const int n_point = 100;

	for(int i = 0; i < n_point + 1; i++)
	{
		const double x = static_cast<double>(i) / n_point; //moves the "." spaces to the right of i, so goes 0 to 0.01 ... 0.99 to 1.0

		//std::setprecision decides the number of decimal places
		//std::numeric_limits<double>::digits10 - 1
		std::cout << std::fixed  
		<< std::showpos
		<< std::setprecision(4)
		<< x
		<< ", "
		<< std::setprecision(std::numeric_limits<double>::digits10 - 1)
		<< sinewave.value(x)
		<< std::endl;
	}

	return 0;
}
*/

float RandomFloat(float a, float b);
float RandomFloat(float a, float b) {
	float random = ((float)rand()) / (float)RAND_MAX;
	float diff = b - a;
	float r = random * diff;
	return a + r;
}

float CubicAmplifier(float input);
float CubicAmplifier(float input)
{
	float output, temp;
	if (input < 0.0)
	{
		temp = input + 1.0f;
		output = (temp * temp * temp) - 1.0f; //cubing the input
	}
	else
	{
		temp = input - 1.0f;
		output = (temp * temp * temp) + 1.0f;
	}

	return output;
}

#define DISTORT(x) CubicAmplifier(CubicAmplifier(CubicAmplifier(CubicAmplifier(x))))
#define DISTORT(x)  CubicAmplifier(CubicAmplifier(CubicAmplifier(CubicAmplifier(x))))
static int gNumNoInputs = 0;


static int filterCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	SAMPLE *out = (SAMPLE*)outputBuffer;
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	unsigned int i;
	(void)timeInfo; // Prevent unused variable warnings. 
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL)
	{
		for (i = 0; i<framesPerBuffer; i++)
		{
			*out++ = 0;  // left - silent  
			*out++ = 0;  // right - silent  
		}
		gNumNoInputs += 1;
	}

	else
	{
		// > 25000 <50000 //cutoff at 50000 seems to be the point where there is little noticable difference for the low pass
		float CUTOFF = 1000.0; //this will low pass but not high pass, maybe 50000 for high pass is like 0 for low pass?
		float RC = 1.0 / (CUTOFF * 2 * 3.14); //calculate rc & dt for the filter
		float dt = 1.0 / SAMPLE_RATE;
		float alpha;

		if (hifilter) {
			alpha = RC / (RC + dt); //high pass
		}
		else {
			alpha = dt / (RC + dt); //low pass
		}

		float filteredArrayL[64];
		float filteredArrayR[64];
		SAMPLE leftinput;
		SAMPLE rightinput;
		SAMPLE prevL;
		SAMPLE prevR;

		for (i = 0; i<framesPerBuffer; i++) {
			leftinput = 0.3f * DISTORT(*in++); //added distortion to hear it better
			rightinput = 0.3f * DISTORT(*in++); //added distortion to hear it better

			if (i == 0) { //ignore filter for first element since we have no previous sample to calculate the filter with
				filteredArrayL[0] = leftinput; //left in 
				filteredArrayR[0] = leftinput; //right in
				*out++ = filteredArrayL[0]; //left out
				*out++ = filteredArrayR[0]; //right out
				prevL = leftinput;
				prevR = leftinput;
			}
			else {
				if (hifilter) { //high pass left & right
					filteredArrayL[i] = alpha * (filteredArrayL[i - 1] + leftinput - prevL);
					filteredArrayR[i] = alpha * (filteredArrayR[i - 1] + leftinput - prevR);
				}
				else { //low pass left & right
					filteredArrayL[i] = filteredArrayL[i - 1] + (alpha*(leftinput - filteredArrayL[i - 1]));
					filteredArrayR[i] = filteredArrayR[i - 1] + (alpha*(leftinput - filteredArrayR[i - 1]));
				}

				*out++ = filteredArrayL[i]; //left out
				*out++ = filteredArrayR[i]; //right out
				prevL = leftinput; //save previous
				prevR = leftinput; //save previous
			}
		}
	}
	return paContinue;
}

static int cleanCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
	SAMPLE *out = (SAMPLE*)outputBuffer;
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	unsigned int i;
	(void)timeInfo; // Prevent unused variable warnings
	(void)statusFlags;
	(void)userData;
	if (inputBuffer == NULL) {

		for (i = 0; i < framesPerBuffer; i++) {
			*out++ = 0;  // left - silent 
			*out++ = 0;  // right - silent 
		}
		gNumNoInputs += 1;
	}
	else {
		for (i = 0; i < framesPerBuffer; i++) {

			SAMPLE leftInput, rightInput;
			leftInput = *in++;
			rightInput = *in++;

			//allow user to add distortion if they click the icon
			if (fuzzEngaged) {
				*out++ = 0.5f * DISTORT(leftInput); // Get interleaved samples from input buffer.
				*out++ = 0.5f * DISTORT(leftInput);
			}
			else {
				*out++ = 0.7f * (leftInput); // Get interleaved samples from input buffer.
				*out++ = 0.7f * (leftInput);
			}
		}
	}return paContinue;
}

//looper still has problems you need to fix, can only be used once in a session, no deleting of buffer on exit, 
static int loopCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
	SAMPLE *out = (SAMPLE*)outputBuffer;
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	SAMPLE leftInput, rightInput, loopinginput;
	unsigned int i;
	(void)timeInfo; // Prevent unused variable warnings
	(void)statusFlags;
	(void)userData;
	if (inputBuffer == NULL) {
		for (i = 0; i < framesPerBuffer; i++) {
			*out++ = 0;  // left - silent 
			*out++ = 0;  // right - silent  
		}
		gNumNoInputs += 1;
	}
	else {
		for (i = 0; i < framesPerBuffer; i++) {

			if (fuzzEngaged) { //one of the few effects I allow to be used with distortion 
				leftInput = 0.5f * DISTORT(*in++); // Get interleaved samples from input buffer.
				rightInput = 0.5f * DISTORT(*in++);
			}
			else {
				leftInput = 0.7f * (*in++); // Get interleaved samples from input buffer.
				rightInput = 0.7f * (*in++);
			}

			if (recording == true && looperqueue.size() <= 441000) { //max 10 seconds of loop for performance concerns
				looperqueue.push(leftInput);
			}
			else if (recording == true && looperqueue.size() > 441000) {
				recording = false;
				looping = true;
			}
			if (looping == true) {
				leftInput += (0.75f * looperqueue.front()); //add the looped data to output
				rightInput += (0.75f * looperqueue.front());//add the looped data to output
				loopinginput = looperqueue.front(); //put the front of the loop to the back of the loop

				if (overdub == true && overdub_length <= looperqueue.size()) { //need to check to make sure that the overdub doesen'

					if (leftInput > loopinginput) { //only overdub samples which are louder than the previous ones
						loopinginput = leftInput; //could this help?
					}
					overdub_length++;
				}
				else {
					overdub = false;
					overdub_length = 0;
				}

				looperqueue.push(loopinginput);
				looperqueue.pop(); //remove from back of queue
			}
			
			

			*out++ = leftInput;
			*out++ = leftInput;
		}
	}return paContinue;
}

static int octfuzzCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData)
{
	SAMPLE *out = (SAMPLE*)outputBuffer; //SAMPLE just means float in this case, don't quite get the pointer stuff yet :-/
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	SAMPLE leftInput, rightInput;
	unsigned int i;
	(void)timeInfo; /* Prevent unused variable warnings. */
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL)
	{
		for (i = 0; i<framesPerBuffer; i++)
		{
			*out++ = 0;  // left - silent 
			*out++ = 0;  // right - silent 
		}
		gNumNoInputs += 1;
	}
	else
	{
		for (i = 0; i<framesPerBuffer; i++)
		{
			leftInput = DISTORT(*in++);
			rightInput = DISTORT(*in++);
			*out++ = leftInput * leftInput; // input is not stereo so I copy the left channel
			*out++ = leftInput * leftInput;
		}
	}

	return paContinue;
}

static int delayCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
	SAMPLE *out = (SAMPLE*)outputBuffer; //what is going on here? what is the (SAMPLE*) before outputbuffer?
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	SAMPLE leftInput, rightInput, delay1;
	unsigned int i;
	(void)timeInfo; /* Prevent unused variable warnings. */
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL) {
		//could I send this audiostream to matlab?
		for (i = 0; i < framesPerBuffer; i++)
		{
			*out++ = 0;  /* left - silent */
			*out++ = 0;  /* right - silent */
		}
		gNumNoInputs += 1;
	}
	else {
		for (i = 0; i < framesPerBuffer; i++)
		{
			if (fuzzEngaged) {
				leftInput = 1.5f * DISTORT(*in++); // Get interleaved samples from input buffer.
				rightInput = 1.5f * DISTORT(*in++);
			}
			else {
				leftInput = 2.0f * (*in++); // Get interleaved samples from input buffer.
				rightInput = 2.0f * (*in++);
			}

			//delayqueue[14000] += (0.4f * rightInput);
			//delayqueue.push_back(0.2f *(rightInput));

			delayqueue[14000] += (0.4f * leftInput);
			delayqueue.push_back(0.2f *(leftInput));
			//delayqueue.push_back(0.4f *(rightInput));
			//delayqueue2.push(0.25f *(rightInput));

			//*out++ = leftInput * rightInput; // ring modulation left
			//*out++ = 0.5f * (leftInput + rightInput); // mixing right
			delay1 = delayqueue.front();
			//delay2 = delayqueue2.front();

			delayqueue.pop_front();

			*out++ = 0.3f * (leftInput + delay1);
			*out++ = 0.3f * (leftInput + delay1);


			//this code gives an interesting  bit-crusher sound
			//*out++ = (left_phase *(*in++));  // left
			//*out++ = (right_phase *(*in++));  // right
			//*out++ = left_phase;  // left 
			//*out++ = right_phase; // right 
			//left_phase += 0.02f;
			// When signal reaches top, drop back down.
			//if (left_phase >= 10.0f) left_phase -= 2.0f;
			// higher pitch so we can distinguish left and right.
			//right_phase += 0.02f;
			//if (right_phase >= 10.0f) right_phase -= 2.0f;
		}


	}return paContinue;
}

static int bitmapCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
	SAMPLE *out = (SAMPLE*)outputBuffer; //what is going on here? what is the (SAMPLE*) before outputbuffer?
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	unsigned int i;
	(void)timeInfo; /* Prevent unused variable warnings. */
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL) {
		//could I send this audiostream to matlab?
		for (i = 0; i < framesPerBuffer; i++)
		{
			*out++ = 0;  /* left - silent */
			*out++ = 0;  /* right - silent */
		}
		gNumNoInputs += 1;
	}
	else {
		for (i = 0; i < framesPerBuffer; i++)
		{
			SAMPLE leftinput, rightinput;
			leftinput = *in++;
			rightinput = *in++;

			if (bitmapchange < 6300) {
				bitmapchange++;

			}
			else {
				bitmapchange = 0;
				bitmappitch = RandomFloat(0.0, 0.05);
				bitmappitchr = RandomFloat(0.0, 0.05);
			}
			//this code gives an interesting  bit-crusher sound
			//this code also can work as a tremolo if you slow it down
			//I need a sine wave through for it to be effective
			//low pass filter need everaging out of of sample values to remove details

			/*
			*out++ = 6.0f * (left_phase *(*in++)); //left
			*out++ = 6.0f * (right_phase *(*in++));  // right
			*/

			*out++ = 1.5f * (left_phase *(leftinput)); //left
													   //*out++ = 5.0f * (*in++);  // left
			*out++ = 1.5f * (right_phase *(leftinput));  // right

														 //*out++ = left_phase;  // left 
														 //*out++ = right_phase; // right 

														 //left_phase += 0.04f;
			left_phase += bitmappitchr;
			//perhaps I could randomise the increment amount to give it a more random sound
			// When signal reaches top, drop back down.
			if (left_phase >= 2.0f) left_phase -= 4.0f;

			// higher pitch so we can distinguish left and right.
			//right_phase += 0.03f;
			right_phase += bitmappitch;
			if (right_phase >= 2.0f) right_phase -= 4.0f;
		}
	}return paContinue;
}

static int ringCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
	SAMPLE *out = (SAMPLE*)outputBuffer; //what is going on here? what is the (SAMPLE*) before outputbuffer?
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	unsigned int i;
	(void)timeInfo; /* Prevent unused variable warnings. */
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL) {
		//could I send this audiostream to matlab?
		for (i = 0; i < framesPerBuffer; i++)
		{
			*out++ = 0;  /* left - silent */
			*out++ = 0;  /* right - silent */
		}
		gNumNoInputs += 1;
	}
	else {
		for (i = 0; i < framesPerBuffer; i++)
		{
			SAMPLE leftInput, rightInput;
			leftInput = *in++;
			rightInput = *in++;
			//this code gives an interesting  bit-crusher sound
			//this code also can work as a tremolo if you slow it down
			//I need a sine wave through for it to be effective
			//low pass filter need everaging out of of sample values to remove details
			*out++ = 8.0f * (left_phase *(leftInput));  // left
			*out++ = 8.0f * (right_phase *(leftInput));  // right
														 //*out++ = left_phase;  // left 
														 //*out++ = right_phase; // right 
			left_phase += 0.01f;
			//perhaps I could randomise the increment amount to give it a more random sound
			// When signal reaches top, drop back down.
			if (left_phase >= 1.0f) left_phase -= 2.0f;
			// higher pitch so we can distinguish left and right.
			right_phase += 0.03f;
			if (right_phase >= 1.0f) right_phase -= 2.0f;
		}
	}return paContinue;
}

static int tremoloCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
	SAMPLE *out = (SAMPLE*)outputBuffer; //what is going on here? what is the (SAMPLE*) before outputbuffer?
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	SAMPLE leftinput, rightinput;
	unsigned int i;
	(void)timeInfo; /* Prevent unused variable warnings. */
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL) {
		//could I send this audiostream to matlab?
		for (i = 0; i < framesPerBuffer; i++)
		{
			*out++ = 0;  // left - silent 
			*out++ = 0;  // right - silent 
		}
		gNumNoInputs += 1;
	}
	else {
		for (i = 0; i < framesPerBuffer; i++)
		{
			leftinput = 0.7f * DISTORT(*in++);
			rightinput = 0.7f * DISTORT(*in++);
			*out++ = left_phase *(leftinput);
			*out++ = left_phase *(leftinput);  // no stereo input, set both to left

			if (goingup == true) {
				//we want to increment until we hit 1
				left_phase += 0.0003f;
				if (left_phase >= 1.0f) goingup = false;

			}
			else {
				//if we're going down the wave we want to decrement the phase
				left_phase -= 0.0003f;
				if (left_phase <= 0.0f) goingup = true;
			}
		}
	}return paContinue;
}

//this isn't really a chorus effect, just a sweeping filter, need to research more into chorus algorithms
static int chorusCallback(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo* timeInfo, PaStreamCallbackFlags statusFlags, void *userData) {
	/*
	SAMPLE *out = (SAMPLE*)outputBuffer; //what is going on here? what is the (SAMPLE*) before outputbuffer?
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	SAMPLE leftinput, rightinput;
	unsigned int i;
	(void)timeInfo; //Prevent unused variable warnings. 
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL) {
		for (i = 0; i < framesPerBuffer; i++) {
			*out++ = 0;  // left - silent 
			*out++ = 0;  // right - silent 
		}gNumNoInputs += 1;
	}
	else {
		float filteredArrayL[64];
		float filteredArrayR[64];
		SAMPLE leftinput;
		SAMPLE rightinput;
		SAMPLE prevL;
		SAMPLE prevR;

		for (i = 0; i < framesPerBuffer; i++) { //a filter sweeping up and down
			leftinput = 0.3f * DISTORT(*in++);
			rightinput = 0.3f * DISTORT(*in++);
			if (i == 0) {
				filteredArrayL[0] = leftinput; //left in, added DISTORT to hear it better
				filteredArrayR[0] = leftinput; //right in, added DISTORT to hear it better
				*out++ = filteredArrayL[0]; //left out
				*out++ = filteredArrayR[0]; //right out
				prevL = leftinput;
				prevR = leftinput;
			}
			else {
				float RC = 1.0 / (chorus_cutoff * 2 * 3.14);//setting up the filter
				float dt = 1.0 / SAMPLE_RATE;
				float alpha;

				alpha = dt / (RC + dt); //only low pass sweeping up and down

				filteredArrayL[i] = filteredArrayL[i - 1] + (alpha*(leftinput - filteredArrayL[i - 1])); //left in 
				filteredArrayR[i] = filteredArrayR[i - 1] + (alpha*(leftinput - filteredArrayR[i - 1])); //right in

				*out++ = filteredArrayL[i]; //left out
				*out++ = filteredArrayR[i]; //right out
				prevL = leftinput; //save previous
				prevR = leftinput; //save previous

				if (goingup == true) {
					chorus_cutoff += 0.05f;
					if (chorus_cutoff >= 2000.0f) goingup = false;
				}
				else {
					chorus_cutoff -= 0.05f;
					if (chorus_cutoff <= 200.0f) goingup = true;
				}
			}
		}
	}
	return paContinue;
	*/

	SAMPLE *out = (SAMPLE*)outputBuffer; //what is going on here? what is the (SAMPLE*) before outputbuffer?
	const SAMPLE *in = (const SAMPLE*)inputBuffer;
	SAMPLE leftInput, rightInput, delay1;
	unsigned int i;
	(void)timeInfo; /* Prevent unused variable warnings. */
	(void)statusFlags;
	(void)userData;

	if (inputBuffer == NULL) {
		//could I send this audiostream to matlab?
		for (i = 0; i < framesPerBuffer; i++)
		{
			*out++ = 0;  /* left - silent */
			*out++ = 0;  /* right - silent */
		}
		gNumNoInputs += 1;
	}
	else {
		for (i = 0; i < framesPerBuffer; i++)
		{
			if (fuzzEngaged) {
				leftInput = 1.5f * DISTORT(*in++); // Get interleaved samples from input buffer.
				rightInput = 1.5f * DISTORT(*in++);
			}
			else {
				leftInput = 2.0f * (*in++); // Get interleaved samples from input buffer.
				rightInput = 2.0f * (*in++);
			}

			//delayqueue[14000] += (0.4f * rightInput);
			//delayqueue.push_back(0.2f *(rightInput));

			//chorusqueue[330] += (0.4f * leftInput);
			chorusqueue.push_back(0.7f *(leftInput));
			//delayqueue.push_back(0.4f *(rightInput));
			//delayqueue2.push(0.25f *(rightInput));

			//*out++ = leftInput * rightInput; // ring modulation left
			//*out++ = 0.5f * (leftInput + rightInput); // mixing right
			delay1 = chorusqueue.front();
			//delay2 = delayqueue2.front();

			chorusqueue.pop_front();

			if (delay1 > leftInput) {
				*out++ = 0.3f * (delay1);
				*out++ = 0.3f * (delay1);
			}
			else{
				*out++ = 0.3f * (leftInput);
				*out++ = 0.3f * (leftInput);
			}
		}
	}return paContinue;
}

//************************************************all my effect switching functions are here**************************************************
static void startswitch() { //method to start the stream

	err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
	loopeng = false; //was using this to try and fix the looper error I was having
	pedalactive = false;//setting this to false after every switch could fix that double click problem

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

static void looperswitch() {
	if (pedalactive == false) {
		printf("DISTORT on .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, loopCallback, NULL);
		looping = false;
		loopeng = true;
		pedalactive = true;
	}
	else {
		printf("DISTORT off .\n");
		loopeng = false;
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
		loopdeleting = true; // need to add this to all of the switch methods
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}
//switch delay on/off
static void delayswitch() {
	while (delayqueue.size() != 28000) {
		if (delayqueue.size() > 28000)
			delayqueue.pop_front();
		else
			delayqueue.push_back(0);
	}
	if (pedalactive == false) {

		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, delayCallback, NULL);
		pedalactive = true;
		loopeng = false;
	}
	else {
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

static void octfuzzswitch() {
	if (pedalactive == false) {
		printf("DISTORT on .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, octfuzzCallback, NULL);
		pedalactive = true;
	}
	else {
		printf("DISTORT off .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
		loopeng = false;
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

//mapped to Autowah pedal (filter), not automatic yet, need a band pass filter first
static void filterswitch() {
	if (pedalactive == false) {
		printf("DISTORT on .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, filterCallback, NULL);
		pedalactive = true;
	}
	else {
		printf("DISTORT off .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
		loopeng = false;
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

static void ringswitch() {
	if (pedalactive == false) {
		printf("DISTORT on .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, ringCallback, NULL);
		pedalactive = true;
	}
	else {
		printf("DISTORT off .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
		loopeng = false;
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

static void bitmapswitch() {
	if (pedalactive == false) {
		printf("DISTORT on .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, bitmapCallback, NULL);
		pedalactive = true;
	}
	else {
		printf("DISTORT off .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
		loopeng = false;
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

static void tremoloswitch() {
	if (pedalactive == false) {
		printf("DISTORT on .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, tremoloCallback, NULL);
		pedalactive = true;
	}
	else {
		printf("DISTORT off .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
		loopeng = false;
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

//chorus doesen't do much yet, just a filter sweep
static void chorusswitch() {
	if (pedalactive == false) {
		printf("DISTORT on .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, chorusCallback, NULL);
		pedalactive = true;
	}
	else {
		printf("DISTORT off .\n");
		err = Pa_OpenStream(&stream, &inputParameters, &outputParameters, SAMPLE_RATE, FRAMES_PER_BUFFER, 0, cleanCallback, NULL);
		pedalactive = false;
		loopeng = false;
	}

	if (err != paNoError) { error = true; return; }
	err = Pa_StartStream(stream);
	if (err != paNoError) { error = true; return; }
}

VOID Example_DrawImage9(HDC hdc) {
	Graphics graphics(hdc);
	// Create an Image object.
	Image image(L"pedalboard5.jpg");
	// Create a Pen object.
	Pen pen(Color(255, 0, 0, 0), 2);
	// Draw the original source image.
	graphics.DrawImage(&image, 10, 10);
	// Create a Rect object that specifies the destination of the image.
}

//***********************************************************MAIN METHOD*********************************************************************************

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);//constructor? lets you define a procedure and build it later?

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)//all these parameters?
{
	for (int a = 0; a < 28000; a = a + 1) {
		delayqueue.push_back(0);
	}//now we have a queue that's 28,000 in ength, about 630+ milisecoonds of delay for the echo effect

	//*****************************************for chorus
	for (int a = 0; a < 661; a = a + 1) { //I need 15ms  seconds of delay (661 samples) for chorus, 28000 is too long 
		chorusqueue.push_back(0);
	}
	//*****************************************for chorus^

	//i think I'll need a sine wave for the chorus
	//

	goingup = true;

	const wchar_t CLASS_NAME[] = L"Sample Window Class";

	Gdiplus::GdiplusStartupInput gdiplusStartupInput;
	ULONG_PTR gdiplusToken;
	Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	WNDCLASS wc = {};

	wc.lpfnWndProc = WindowProc; //attach this callback procedure to wc
	wc.hInstance = hInstance; //handle to application instance, handle?
	wc.lpszClassName = CLASS_NAME; //class name?

	RegisterClass(&wc); //register wc

						//*************************************************portaudio initialise here********************************************************************
	err = Pa_Initialize();
	if (err != paNoError) error = true;

	inputParameters.device = Pa_GetDefaultInputDevice(); // default input device 
	if (inputParameters.device == paNoDevice) {
		fprintf(stderr, "Error: No default input device.\n");
		//goto error;
		error = true;
	}
	inputParameters.channelCount = 2;       // stereo input 
	inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL;

	outputParameters.device = Pa_GetDefaultOutputDevice(); // default output device 
	if (outputParameters.device == paNoDevice) {
		fprintf(stderr, "Error: No default output device.\n");
		//goto error;
		error = true;
	}
	outputParameters.channelCount = 2;       // stereo output 
	outputParameters.sampleFormat = PA_SAMPLE_TYPE;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	//*************************************************portaudio end initialisation here********************************************************************

	//*************************************************GUI start here********************************************************************
	HWND hwnd = CreateWindowEx(0, CLASS_NAME, L"BIT-RIFF", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL,
		NULL, hInstance, NULL);

	HWND hWndButton = CreateWindowEx(NULL, L"BUTTON", L"Distortion", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 30, 235, 150, 80, hwnd,
		(HMENU)IDC_12TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton1 = CreateWindowEx(NULL, L"BUTTON", L"Bitmap", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 240, 235, 150, 80, hwnd,
		(HMENU)IDC_2ND_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton2 = CreateWindowEx(NULL, L"BUTTON", L"Delay (echo)", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 450, 235, 150, 80, hwnd,
		(HMENU)IDC_3RD_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton3 = CreateWindowEx(NULL, L"BUTTON", L"Tremolo", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 660, 235, 150, 80, hwnd,
		(HMENU)IDC_4TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton4 = CreateWindowEx(NULL, L"BUTTON", L"Octave-Fuzz", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 30, 600, 150, 80, hwnd,
		(HMENU)IDC_5TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton5 = CreateWindowEx(NULL, L"BUTTON", L"Engage/stop looper", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 235, 595, 150, 40, hwnd,
		(HMENU)IDC_6TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton5v2 = CreateWindowEx(NULL, L"BUTTON", L"record/overdub", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 235, 640, 150, 40, hwnd,
		(HMENU)IDC_9TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton6 = CreateWindowEx(NULL, L"BUTTON", L"Ring-Mod", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 450, 600, 150, 80, hwnd,
		(HMENU)IDC_7TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton7 = CreateWindowEx(NULL, L"BUTTON", L"Chorus", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 660, 600, 150, 80, hwnd,
		(HMENU)IDC_8TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton8 = CreateWindowEx(NULL, L"BUTTON", L"Auto-wah", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 860, 235, 75, 80, hwnd,
		(HMENU)IDC_10TH_BUTTON, GetModuleHandle(NULL), NULL);

	HWND hWndButton8v2 = CreateWindowEx(NULL, L"BUTTON", L"hi/lo", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 935, 235, 75, 80, hwnd,
		(HMENU)IDC_11TH_BUTTON, GetModuleHandle(NULL), NULL);

	if (hwnd == NULL) {
		return 0;
	}
	ShowWindow(hwnd, nCmdShow);
	ShowWindow(hWndButton, nCmdShow);
	ShowWindow(hWndButton1, nCmdShow);
	ShowWindow(hWndButton2, nCmdShow);
	ShowWindow(hEdit, nCmdShow);
	// Run the message loop.
	//*************************************************GUI in main ends here********************************************************************

	error = false;
	pedalactive = true;
	//will open stream at start of program by default
	startswitch();
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0)) //the msg loop, kinda know what this does
	{
		if (error) {
			break;
		}
		else {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (error) {
		Pa_Terminate();
		Gdiplus::GdiplusShutdown(gdiplusToken);
		fprintf(stderr, "An error occured while using the portaudio stream\n");
		fprintf(stderr, "Error number: %d\n", err);
		fprintf(stderr, "Error message: %s\n", Pa_GetErrorText(err));
		return -1;

	}

	printf("Finished. gNumNoInputs = %d\n", gNumNoInputs);
	Pa_Terminate(); //terminate portaudio command
	Gdiplus::GdiplusShutdown(gdiplusToken);
	return 0;
}

//****************************************************windows callback function*********************************************************************

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {//callback procedure for this window, takes in all the window details
	switch (uMsg) {
		case WM_DESTROY: //if the user quits, hit the x on top right corner
			err = Pa_CloseStream(stream);
			_CrtDumpMemoryLeaks();
			PostQuitMessage(0);
			return 0;

		case WM_PAINT: {//once the window is created we paint it all white
			PAINTSTRUCT ps;

			HDC hdc = BeginPaint(hwnd, &ps);
			FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
			Example_DrawImage9(hdc);//added this line
			EndPaint(hwnd, &ps);

			return 0;
		}
		case WM_COMMAND: {
			switch (LOWORD(wParam)) {

				//case IDC_MAIN_BUTTON:{}  not used

				case IDC_2ND_BUTTON: { //bitmap here
					loopdeleting = true;
					loopeng = false;
					err = Pa_CloseStream(stream); //close old stream
					left_phase = 0;
					right_phase = 0;
					bitmapswitch(); //open bitmap
					break;
				}

				case IDC_3RD_BUTTON: { //delay
					loopdeleting = true;
					loopeng = false;
					err = Pa_CloseStream(stream); //close the previous stream
					left_phase = 0; //reset the phase for tremolo in case switching from it
					right_phase = 0;
					delayswitch();
					break; //break of switch(uMsg);
				}
				case IDC_4TH_BUTTON: { // tremolo
					loopdeleting = true;
					loopeng = false;
					err = Pa_CloseStream(stream); //this needs to go inside the callback procedure
					left_phase = 0;
					right_phase = 0;
					tremoloswitch();
					break;
				}
				case IDC_5TH_BUTTON: { //octave DISTORT
					loopdeleting = true;
					loopeng = false;
					err = Pa_CloseStream(stream); //this needs to go inside the callback procedure
					left_phase = 0;
					right_phase = 0;
					octfuzzswitch();
					break;
				}
				case IDC_6TH_BUTTON: { //engage looper
					err = Pa_CloseStream(stream); //this needs to go inside the callback procedure
					left_phase = 0;
					right_phase = 0;
					if (looperqueue.size() > 0) {
						//MessageBox(NULL, L"Restart Application to use looper again", L"Information", MB_ICONINFORMATION);
						MessageBox(NULL, L"looper cleared, ready to be used again", L"Information", MB_ICONINFORMATION);
						//I replace looperqueue with a blank version
						//don't have time to get this working properly
						looperqueue = blankqueue;
						//loopdeleting = false;
						//loopeng = false;
						firstrecording = true;
						looping = false;
					
						looperswitch();
					}
					else {
						looperswitch();
					}
					break;
				}
				case IDC_9TH_BUTTON: { //looper record button
					if (loopeng == false) {
						MessageBox(NULL, L"Looper not engaged!!!.", L"Information", MB_ICONINFORMATION);
					}
					else {
						if (recording == false && firstrecording == true) {
							recording = true;
							firstrecording = false;
						}
						else {
							if (looping == true && overdub == false) {
								overdub = true; //we engage overdub if there already is a recording
							}
							else {
								recording = false;
								overdub = false;
								looping = true; //now the only way to turn looping off is to disengage the looper
							}
						}
					}
					break;
				}
				case IDC_7TH_BUTTON: { // ring modulator
					loopdeleting = true;
					loopeng = false;
					err = Pa_CloseStream(stream);
					left_phase = 0;
					right_phase = 0;
					ringswitch();
					break;
				}
				case IDC_8TH_BUTTON: { //chorus
					loopdeleting = true;
					loopeng = false;
					err = Pa_CloseStream(stream);
					left_phase = 0;
					right_phase = 0;
					chorusswitch();
					break;
				}
				case IDC_10TH_BUTTON: { //filter (engage)
					loopdeleting = true;
					loopeng = false;
					err = Pa_CloseStream(stream); //this needs to go inside the callback procedure
					left_phase = 0; //reset the phaser
					right_phase = 0;
					filterswitch();
					break;
				}
				case IDC_11TH_BUTTON: { //filter (switch hi/lo pass)
					if (hifilter) {
						hifilter = false;
					}
					else {
						hifilter = true;
					}
					break;
				}
				case IDC_12TH_BUTTON: { //switch for DISTORT (distortion)
					if (fuzzEngaged) {
						fuzzEngaged = false;
					}
					else {
						fuzzEngaged = true;
					}
					break;
				}
				break;
			}
			break;
			return 0;
		}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
