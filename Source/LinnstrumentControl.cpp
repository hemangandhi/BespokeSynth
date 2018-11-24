/*
  ==============================================================================

    LinnstrumentControl.cpp
    Created: 28 Oct 2018 2:17:18pm
    Author:  Ryan Challinor

  ==============================================================================
*/

#include "LinnstrumentControl.h"
#include "SynthGlobals.h"
#include "IAudioSource.h"
#include "ModularSynth.h"
#include "FillSaveDropdown.h"
#include "ModulationChain.h"
#include "PolyphonyMgr.h"
#include "MidiController.h"

LinnstrumentControl::LinnstrumentControl()
: mControllerIndex(-1)
, mControllerList(nullptr)
, mDevice(this)
, mDecayMs(500)
, mDecaySlider(nullptr)
, mRequestedOctaveTime(0)
, mBlackout(false)
, mBlackoutCheckbox(nullptr)
, mLinnstrumentOctave(5)
{
   TheScale->AddListener(this);
}

LinnstrumentControl::~LinnstrumentControl()
{
   TheScale->RemoveListener(this);
}

void LinnstrumentControl::CreateUIControls()
{
   IDrawableModule::CreateUIControls();
   mControllerList = new DropdownList(this,"controller",5,5,&mControllerIndex);
   mDecaySlider = new FloatSlider(this,"decay",mControllerList,kAnchor_Below,100,15,&mDecayMs,0,2000);
   mBlackoutCheckbox = new Checkbox(this, "blackout", mDecaySlider, kAnchor_Below, &mBlackout);
}

void LinnstrumentControl::Init()
{
   IDrawableModule::Init();
   
   InitController();
}

void LinnstrumentControl::Poll()
{
   for (int i=0; i<128; ++i)
      mNoteAge[i].Update(i, this);
   
   if (gTime - mRequestedOctaveTime > 2000)
   {
      SendNRPN(299, 36);   //request left split octave
      mRequestedOctaveTime = gTime;
   }
}

void LinnstrumentControl::InitController()
{
   BuildControllerList();
   
   const std::vector<string>& devices = mDevice.GetPortList(false);
   for (int i=0; i<devices.size(); ++i)
   {
      if (strstr(devices[i].c_str(), "LinnStrument") != nullptr)
      {
         mControllerIndex = i;
         mDevice.ConnectOutput(i);
         mDevice.ConnectInput(devices[i].c_str());
         UpdateScaleDisplay();
         break;
      }
   }
}

void LinnstrumentControl::DrawModule()
{
   if (Minimized() || IsVisible() == false)
      return;
   
   mControllerList->Draw();
   mDecaySlider->Draw();
   mBlackoutCheckbox->Draw();
}

void LinnstrumentControl::BuildControllerList()
{
   mControllerList->Clear();
   const std::vector<string>& devices = mDevice.GetPortList(false);
   for (int i=0; i<devices.size(); ++i)
      mControllerList->AddLabel(devices[i].c_str(), i);
}

void LinnstrumentControl::OnScaleChanged()
{
   UpdateScaleDisplay();
}

void LinnstrumentControl::UpdateScaleDisplay()
{
   //SendScaleInfo();
   
   for (int y=0; y<8; ++y)
   {
      mDevice.SendCC(21, y);
      for (int x=0; x<25; ++x)
      {
         mDevice.SendCC(20, x + 1);
         mDevice.SendCC(22, GetGridColor(x, y));
      }
   }
}

void LinnstrumentControl::SetGridColor(int x, int y, int color)
{
   mDevice.SendCC(21, y);
   mDevice.SendCC(20, x + 1);
   mDevice.SendCC(22, color);
}

int LinnstrumentControl::GetGridColor(int x, int y)
{
   if (mBlackout)
      return kLinnColor_Black;
   
   int pitch = GridToPitch(x,y);
   if (TheScale->IsRoot(pitch))
      return kLinnColor_Green;
   if (TheScale->IsInPentatonic(pitch))
      return kLinnColor_Orange;
   if (TheScale->IsInScale(pitch))
      return kLinnColor_Yellow;
   return kLinnColor_Black;
}

int LinnstrumentControl::GridToPitch(int x, int y)
{
   return 30 + x + y * 5 + (mLinnstrumentOctave - 5) * 12;
}

void LinnstrumentControl::SetPitchColor(int pitch, int color)
{
   for (int y=0; y<8; ++y)
   {
      for (int x=0; x<25; ++x)
      {
         if (GridToPitch(x, y) == pitch)
         {
            if (color == 0)   //use whatever the color would be
               color = GetGridColor(x,y);
            SetGridColor(x, y, color);
         }
      }
   }
}

void LinnstrumentControl::SendScaleInfo()
{
   /*NRPN format:
   send these 6 messages in a row:
   1011nnnn   01100011 ( 99)  0vvvvvvv         NRPN parameter number MSB CC
   1011nnnn   01100010 ( 98)  0vvvvvvv         NRPN parameter number LSB CC
   1011nnnn   00000110 (  6)  0vvvvvvv         NRPN parameter value MSB CC
   1011nnnn   00100110 ( 38)  0vvvvvvv         NRPN parameter value LSB CC
   1011nnnn   01100101 (101)  01111111 (127)   RPN parameter number Reset MSB CC
   1011nnnn   01100100 (100)  01111111 (127)   RPN parameter number Reset LSB CC
   
   Linnstrument NRPN scale messages:
   203    0-1     Global Main Note Light C (0: Off, 1: On)
   204    0-1     Global Main Note Light C# (0: Off, 1: On)
   205    0-1     Global Main Note Light D (0: Off, 1: On)
   206    0-1     Global Main Note Light D# (0: Off, 1: On)
   207    0-1     Global Main Note Light E (0: Off, 1: On)
   208    0-1     Global Main Note Light F (0: Off, 1: On)
   209    0-1     Global Main Note Light F# (0: Off, 1: On)
   210    0-1     Global Main Note Light G (0: Off, 1: On)
   211    0-1     Global Main Note Light G# (0: Off, 1: On)
   212    0-1     Global Main Note Light A (0: Off, 1: On)
   213    0-1     Global Main Note Light A# (0: Off, 1: On)
   214    0-1     Global Main Note Light B (0: Off, 1: On)
   215    0-1     Global Accent Note Light C (0: Off, 1: On)
   216    0-1     Global Accent Note Light C# (0: Off, 1: On)
   217    0-1     Global Accent Note Light D (0: Off, 1: On)
   218    0-1     Global Accent Note Light D# (0: Off, 1: On)
   219    0-1     Global Accent Note Light E (0: Off, 1: On)
   220    0-1     Global Accent Note Light F (0: Off, 1: On)
   221    0-1     Global Accent Note Light F# (0: Off, 1: On)
   222    0-1     Global Accent Note Light G (0: Off, 1: On)
   223    0-1     Global Accent Note Light G# (0: Off, 1: On)
   224    0-1     Global Accent Note Light A (0: Off, 1: On)
   225    0-1     Global Accent Note Light A# (0: Off, 1: On)
   226    0-1     Global Accent Note Light B (0: Off, 1: On)
   */
   
   const unsigned char setMainNoteBase = 203;
   const unsigned char setAccentNoteBase = 215;
   if (TheScale->GetTet() == 12) //linnstrument only works with 12-tet
   {
      for (int pitch = 0; pitch < 12; ++pitch)
      {
         //set main note
         int number = setMainNoteBase + pitch;
         SendNRPN(number, TheScale->IsInScale(pitch));
         
         usleep(10000);
         
         //set accent note
         number = setAccentNoteBase + pitch;
         SendNRPN(number, TheScale->IsRoot(pitch));
         
         usleep(10000);
      }
   }
}

void LinnstrumentControl::SendNRPN(int param, int value)
{
   const unsigned char channelHeader = 177;
   mDevice.SendData(channelHeader, 99, param >> 7);
   mDevice.SendData(channelHeader, 98, param & 0x7f);
   mDevice.SendData(channelHeader, 6, value >> 7);
   mDevice.SendData(channelHeader, 38, value & 0x7f);
   mDevice.SendData(channelHeader, 101, 127);
   mDevice.SendData(channelHeader, 100, 127);
}

void LinnstrumentControl::PlayNote(double time, int pitch, int velocity, int voiceIdx, ModulationParameters modulation)
{
   mModulators[voiceIdx] = modulation;
   
   if (pitch >= 0 && pitch < 128)
   {
      mNoteAge[pitch].mTime = velocity > 0 ? -1 : time;
      if (velocity > 0)
         mNoteAge[pitch].mVoiceIndex = voiceIdx;
      mNoteAge[pitch].Update(pitch, this);
   }
}

void LinnstrumentControl::OnMidiNote(MidiNote& note)
{
   mDevice.SendNote(note.mPitch, 0, false, note.mChannel); //don't allow linnstrument to light played notes
}

void LinnstrumentControl::OnMidiControl(MidiControl& control)
{
   if (control.mControl == 101)
      mLastReceivedNRPNParamMSB = control.mValue;
   if (control.mControl == 100)
      mLastReceivedNRPNParamLSB = control.mValue;
   if (control.mControl == 6)
      mLastReceivedNRPNValueMSB = control.mValue;
   if (control.mControl == 38)
   {
      mLastReceivedNRPNValueLSB = control.mValue;
      
      int nrpnParam = mLastReceivedNRPNParamMSB << 7 | mLastReceivedNRPNParamLSB;
      int nrpnValue = mLastReceivedNRPNValueMSB << 7 | mLastReceivedNRPNValueLSB;
      
      if (nrpnParam == 36)
         mLinnstrumentOctave = nrpnValue;
   }
}

void LinnstrumentControl::DropdownUpdated(DropdownList* list, int oldVal)
{
   if (list == mControllerList)
   {
      mDevice.ConnectOutput(mControllerIndex);
      UpdateScaleDisplay();
   }
}

void LinnstrumentControl::DropdownClicked(DropdownList* list)
{
   BuildControllerList();
}

void LinnstrumentControl::CheckboxUpdated(Checkbox* checkbox)
{
   if (checkbox == mBlackoutCheckbox)
      UpdateScaleDisplay();
}

void LinnstrumentControl::LoadLayout(const ofxJSONElement& moduleInfo)
{
   SetUpFromSaveData();
}

void LinnstrumentControl::SetUpFromSaveData()
{
   InitController();
}

void LinnstrumentControl::NoteAge::Update(int pitch, LinnstrumentControl* linnstrument)
{
   float age = gTime - mTime;
   int newColor;
   if (mTime < 0 || age < 0)
      newColor = kLinnColor_Red;
   else if (age < linnstrument->mDecayMs * .25f)
      newColor = kLinnColor_Pink;
   else if (age < linnstrument->mDecayMs * .5f)
      newColor = kLinnColor_Cyan;
   else if (age < linnstrument->mDecayMs)
      newColor = kLinnColor_Blue;
   else
      newColor = kLinnColor_Off;
   
   if (mVoiceIndex != -1 && linnstrument->mModulators[mVoiceIndex].pitchBend)
   {
      float bendRounded = round(linnstrument->mModulators[mVoiceIndex].pitchBend->GetValue(0));
      pitch += (int)bendRounded;
   }
   
   if (newColor != mColor || pitch != mOutputPitch)
   {
      if (pitch != mOutputPitch)
         linnstrument->SetPitchColor(mOutputPitch, kLinnColor_Off);
      mColor = newColor;
      mOutputPitch = pitch;
      linnstrument->SetPitchColor(pitch, newColor);
   }
}
