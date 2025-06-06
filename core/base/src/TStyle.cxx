// @(#)root/base:$Id$
// Author: Rene Brun   12/12/94

/*************************************************************************
 * Copyright (C) 1995-2000, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <cstring>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <iostream>
#include <fstream>

#include "strlcpy.h"
#include "TApplication.h"
#include "TColor.h"
#include "TDatime.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TVirtualPad.h"
#include "TVirtualMutex.h"
#include "TEnv.h"

TStyle  *gStyle;
const UInt_t kTakeStyle = BIT(17);

ClassImp(TStyle);

/** \class TStyle
\ingroup Base
 \ingroup GraphicsAtt

TStyle objects may be created to define special styles.
By default ROOT creates a default style that can be accessed via
the gStyle pointer.

This class includes functions to set some of the following object attributes.
  - Canvas
  - Pad
  - Histogram axis
  - Lines
  - Fill areas
  - Text
  - Markers
  - Functions
  - Histogram Statistics and Titles

All objects that can be drawn in a pad inherit from one or more attribute classes
like TAttLine, TAttFill, TAttText, TAttMarker. When the objects are created, their
default attributes are taken from the current style. The current style is an object
of the class[TStyle](https://root.cern/doc/master/classTStyle.html) and can be
referenced via the global variable `gStyle` (in TStyle.h).

ROOT provides two styles called "Default" and "Plain". The "Default"
style is created simply by:

~~~ {.cpp}
    auto default = new TStyle("Default","Default Style");
~~~

The `Plain` style can be used if you are working on a monochrome display or
if you want to get a "conventional" Postscript output. These are the instructions
in the ROOT constructor to create the `Plain` style.

```
auto plain  = new TStyle("Plain","Plain Style (no colors/fill areas)");

   plain->SetCanvasBorderMode(0);
   plain->SetPadBorderMode(0);
   plain->SetPadColor(0);
   plain->SetCanvasColor(0);
   plain->SetTitleColor(0);
   plain->SetStatColor(0);
```

You can set the current style with:

```
gROOT->SetStyle(style_name);
```

You can get a pointer to an existing style with:

```
auto style = gROOT->GetStyle(style_name);
```

You can create additional styles with:

```
 TStyle *st1 = new TStyle("st1","my style");
    st1->Set....
    st1->cd();  this becomes now the current style gStyle
```

In your [rootlogon.C](https://root.cern/doc/master/classexamples/startsession.log.html)
file, you can redefine the default parameters via statements like:

```
  gStyle->SetStatX(0.7);
  gStyle->SetStatW(0.2);
  gStyle->SetLabelOffset(1.2);
  gStyle->SetLabelFont(72);
```

Note that when an object is created, its attributes are taken from the current
style. For example, you may have created an histogram in a previous session,
saved it in a file. Meanwhile, if you have changed the style, the histogram will
be drawn with the old attributes. You can force the current style attributes to
be set when you read an object from a file by calling:

```
gROOT->ForceStyle();
```

before reading the objects from the file.

Let's assume you have a canvas or pad with your histogram or any other object,
you can force these objects to get the attributes of the current style via:

```
canvas->UseCurrentStyle();
```

The description of the style functions should be clear from the name of the
TStyle Setters or Getters. Some functions have an extended description, in particular:

  - TStyle:SetLabelFont.
  - TStyle:SetLineStyleString, to set the format of dashed lines.
  - TStyle:SetOptStat.
  - TStyle:SetPalette to change the colors palette.
  - TStyle:SetTitleOffset.

*/

////////////////////////////////////////////////////////////////////////////////
/// Default constructor.

TStyle::TStyle() :TNamed()
{
   Reset();
}

////////////////////////////////////////////////////////////////////////////////
/// Create a new TStyle.
/// The following names are reserved to create special styles:
///
///   - `Classic`: Similar to `Default` style set in TStyle::Reset
///   - `Plain`: a black&white oriented style
///   - `Bold`
///   - `Video`
///   - `Pub`
///   - `Modern`: Used when ROOT starts
///   - `ATLAS`: style used by the ATLAS experiment
///   - `BELLE2`: style used by the BELLE II experiment
///     (see the definition of these styles below).
///
/// Note a side-effect of calling gStyle->SetFillColor(0). This is nearly
/// equivalent of selecting the "Plain" style.
///
/// Many graphics attributes may be set via the TStyle, see in particular
///  - TStyle::SetNdivisions
///  - TStyle::SetAxisColor
///  - TStyle::SetHeaderPS
///  - TStyle::SetTitlePS
///  - TStyle::SetLabelColor
///  - TStyle::SetLabelFont
///  - TStyle::SetLabelOffset
///  - TStyle::SetLabelSize
///  - TStyle::SetOptDate
///  - TStyle::SetLineStyleString
///  - TStyle::SetOptFit
///  - TStyle::SetOptStat
///  - TStyle::SetPaperSize
///  - TStyle::SetTickLength
///  - TStyle::SetTitleOffset
///  - TStyle::SetTitleSize
///  - TStyle::SetPalette
///  - TStyle::SetTimeOffset
///  - TStyle::SetStripDecimals
///
///  The current style is pointed by gStyle.
///
///  When calling myStyle->cd(), gStyle is set to myStyle.
///
///  One can also use gROOT to change the current style, e.g.
///
///    gROOT->SetStyle("Plain") will change the current style gStyle to the
///    "Plain" style
///
///  See also TROOT::ForceStyle and TROOT::UseCurrentStyle

TStyle::TStyle(const char *name, const char *title)
{
   TString style_name = name;

   SetNameTitle(style_name, title);

   // If another style was already created with the same name, it is overwrite.
   delete gROOT->GetStyle(style_name);

   Reset();

   {
      R__LOCKGUARD(gROOTMutex);
      gROOT->GetListOfStyles()->Add(this);
   }

   if (strcmp(style_name,"Modern") == 0) {
      // Modern style
      SetFrameBorderMode(0);
      SetFrameFillColor(0);
      SetCanvasBorderMode(0);
      SetCanvasColor(0);
      SetPadBorderMode(0);
      SetPadColor(0);
      SetStatColor(0);
      SetTitleFont(42,"");
      SetLabelFont(42,"x");
      SetTitleFont(42,"x");
      SetLabelFont(42,"y");
      SetTitleFont(42,"y");
      SetLabelFont(42,"z");
      SetTitleFont(42,"z");
      SetStatFont(42);
      SetLabelSize(0.035,"x");
      SetTitleSize(0.035,"x");
      SetLabelSize(0.035,"y");
      SetTitleSize(0.035,"y");
      SetLabelSize(0.035,"z");
      SetTitleSize(0.035,"z");
      SetTitleSize(0.050,"");
      SetTitleAlign(23);
      SetTitleX(0.5);
      SetTitleBorderSize(0);
      SetTitleFillColor(0);
      SetTitleStyle(0);
      SetTitleOffset(0.,"Y");
      SetStatBorderSize(1);
      SetOptStat(1111);
      SetStatY(0.935);
      SetHistLineColor(kBlue+2);
      SetLegendBorderSize(1);
      SetLegendFillColor(0);
      SetLegendFillStyle(1001);
      SetLegendFont(42);
      SetLegendTextSize(0.);
      SetFuncWidth(2);
      SetFuncColor(2);
   }
   if (strcmp(style_name,"Plain") == 0) {
      // May be a standard style to be initialised
      SetFrameBorderMode(0);
      SetFrameFillColor(0);
      SetCanvasBorderMode(0);
      SetPadBorderMode(0);
      SetPadColor(0);
      SetCanvasColor(0);
      SetTitleFillColor(0);
      SetTitleBorderSize(1);
      SetStatColor(0);
      SetStatBorderSize(1);
      SetLegendBorderSize(1);
      return;
   }
   if (strcmp(style_name,"Bold") == 0) {
      // Authors: Art Poskanzer and Jim Thomas, LBNL, Oct. 2000
      SetPalette(1,nullptr);
      SetCanvasColor(10);
      SetCanvasBorderMode(0);
      SetFrameLineWidth(3);
      SetFrameFillColor(10);
      SetPadColor(10);
      SetPadTickX(1);
      SetPadTickY(1);
      SetPadBottomMargin(0.15);
      SetPadLeftMargin(0.15);
      SetHistLineWidth(3);
      SetHistLineColor(kRed);
      SetFuncWidth(3);
      SetFuncColor(kGreen);
      SetLineWidth(3);
      SetLabelSize(0.05,"xyz");
      SetLabelOffset(0.01,"y");
      SetLabelColor(kBlue,"xy");
      SetTitleSize(0.06,"xyz");
      SetTitleOffset(1.3,"Y");
      SetTitleFillColor(10);
      SetTitleTextColor(kBlue);
      SetStatColor(10);
      return;
   }
   if (strcmp(style_name,"Video") == 0) {
      // Author: Art Poskanzer, LBNL, Oct. 1999
      SetPalette(1,nullptr);
      SetCanvasColor(10);
      SetCanvasBorderMode(0);
      SetFrameLineWidth(3);
      SetFrameFillColor(10);
      SetPadColor(10);
      SetPadTickX(1);
      SetPadTickY(1);
      SetPadBottomMargin(0.2);
      SetPadLeftMargin(0.2);
      SetHistLineWidth(8);
      SetHistLineColor(kRed);
      SetLabelSize(0.06,"xyz");
      SetLabelColor(kBlue,"xyz");
      SetTitleSize(0.08,"xyz");
      SetTitleFillColor(10);
      SetTitleTextColor(kBlue);
      SetStatColor(10);
      SetFuncWidth(8);
      SetFuncColor(kGreen);
      SetLineWidth(3);
      return;
   }
   if (strcmp(style_name,"Pub") == 0) {
      // Authors: Art Poskanzer and Jim Thomas, LBNL, Oct. 2000
      SetOptTitle(0);
      SetOptStat(0);
      SetPalette(8,nullptr);
      SetCanvasColor(10);
      SetCanvasBorderMode(0);
      SetFrameLineWidth(3);
      SetFrameFillColor(10);
      SetPadColor(10);
      SetPadTickX(1);
      SetPadTickY(1);
      SetPadBottomMargin(0.15);
      SetPadLeftMargin(0.15);
      SetHistLineWidth(3);
      SetHistLineColor(kRed);
      SetFuncWidth(3);
      SetFuncColor(kGreen);
      SetLineWidth(3);
      SetLabelSize(0.05,"xyz");
      SetLabelOffset(0.01,"y");
      SetLabelColor(kBlack,"xyz");
      SetTitleSize(0.06,"xyz");
      SetTitleOffset(1.3,"y");
      SetTitleFillColor(10);
      SetTitleTextColor(kBlue);
      return;
   }
   if (strcmp(style_name,"ATLAS") == 0) {
      // Author: M.Sutton - Atlas Collaboration 2010
      SetFrameBorderMode(0);
      SetFrameFillColor(0);
      SetCanvasBorderMode(0);
      SetCanvasColor(0);
      SetPadBorderMode(0);
      SetPadColor(0);
      SetStatColor(0);
      SetPaperSize(20,26);
      SetPadTopMargin(0.05);
      SetPadRightMargin(0.05);
      SetPadBottomMargin(0.16);
      SetPadLeftMargin(0.16);
      SetTitleXOffset(1.4);
      SetTitleYOffset(1.4);
      Int_t font = 42;
      Double_t tsize=0.05;
      SetTextFont(font);
      SetTextSize(tsize);
      SetLabelFont(font,"x");
      SetTitleFont(font,"x");
      SetLabelFont(font,"y");
      SetTitleFont(font,"y");
      SetLabelFont(font,"z");
      SetTitleFont(font,"z");
      SetLabelSize(tsize,"x");
      SetTitleSize(tsize,"x");
      SetLabelSize(tsize,"y");
      SetTitleSize(tsize,"y");
      SetLabelSize(tsize,"z");
      SetTitleSize(tsize,"z");
      SetMarkerStyle(20);
      SetMarkerSize(1.2);
      SetHistLineWidth(2.);
      SetLineStyleString(2,"[12 12]");
      SetEndErrorSize(0.); // get rid of error bar caps
      SetOptTitle(0);
      SetOptStat(0);
      SetOptFit(0);
      SetPadTickX(1);
      SetPadTickY(1);
   }
   if (strcmp(style_name,"BELLE2") == 0) {
      // use plain black on white colors
      Int_t icol=0; // WHITE
      SetFrameBorderMode(icol);
      SetFrameFillColor(icol);
      SetCanvasBorderMode(icol);
      SetCanvasColor(icol);
      SetPadBorderMode(icol);
      SetPadColor(icol);
      SetStatColor(icol);
      //SetFillColor(icol); // don't use: white fill color for *all* objects

      // set the paper & margin sizes
      SetPaperSize(20,26);

      // set margin sizes
      SetPadTopMargin(0.05);
      SetPadRightMargin(0.05);
      SetPadBottomMargin(0.16);
      SetPadLeftMargin(0.16);

      // set title offsets (for axis label)
      SetTitleXOffset(1.0);
      SetTitleYOffset(1.0);

      // use large fonts
      //Int_t font=72; // Helvetica italics
      Int_t font=42; // Helvetica
      Double_t tsize=0.05;
      SetTextFont(font);
      SetTextSize(tsize);

      SetLabelFont(font,"x");
      SetTitleFont(font,"x");
      SetLabelFont(font,"y");
      SetTitleFont(font,"y");
      SetLabelFont(font,"z");
      SetTitleFont(font,"z");

      SetLabelSize(tsize,"x");
      SetTitleSize(.065,"x");
      SetLabelSize(tsize,"y");
      SetTitleSize(.065,"y");
      SetLabelSize(tsize,"z");
      SetTitleSize(.065,"z");

      SetTitleOffset(1.1,"x");
      SetTitleOffset(1.1,"y");
      SetTitleOffset(1.1,"z");

      SetLabelOffset(0.015,"x");
      SetLabelOffset(0.015,"y");
      SetLabelOffset(0.015,"z");

      SetTickLength(0.03,"x");
      SetTickLength(0.02,"y");  // This way we slightly achieve equal length ticks for x and y

      // use bold lines and markers
      SetMarkerStyle(20);
      SetMarkerSize(0.9);
      SetHistLineWidth(2);
      SetLineStyleString(2,"[12 12]"); // postscript dashes

      // get rid of X error bars
      SetErrorX(0.001);
      // get rid of error bar caps
      SetEndErrorSize(0.);

      // do not display any of the standard histogram decorations
      SetOptTitle(0);
      SetOptStat(0);
      SetOptFit(0);

      // put tick marks on top and RHS of plots
      SetPadTickX(0);
      SetPadTickY(0);

      SetFrameLineWidth(2);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Destructor.

TStyle::~TStyle()
{
   R__LOCKGUARD(gROOTMutex);
   gROOT->GetListOfStyles()->Remove(this);
   if (gStyle == this) gStyle = (TStyle*)gROOT->GetListOfStyles()->Last();
}

////////////////////////////////////////////////////////////////////////////////
/// Copy constructor

TStyle::TStyle(const TStyle &style) : TNamed(style), TAttLine(style), TAttFill(style), TAttMarker(style), TAttText(style)
{
   style.TStyle::Copy(*this);
}

////////////////////////////////////////////////////////////////////////////////
/// Assignment operator.

TStyle& TStyle::operator=(const TStyle& style)
{
   if (this != &style)
      style.TStyle::Copy(*this);
   return *this;
}

////////////////////////////////////////////////////////////////////////////////
// Return axis number (1 for X, 2 for Y, 3 for Z), otherwise 0

Int_t TStyle::AxisChoice(Option_t *axis) const
{
   UChar_t a = axis ? *axis : 0;
   a -= (a >= 'x') ? 'x' : 'X'; // toupper and a-='X'; intentional underflow
   return (a > 2) ? 0 : (Int_t)(a+1);
}

////////////////////////////////////////////////////////////////////////////////
/// Browse the style object.

void TStyle::Browse(TBrowser *)
{
   cd();
}

////////////////////////////////////////////////////////////////////////////////
/// Create some standard styles.

void TStyle::BuildStyles()
{
   TColor *col = new TColor(); // force the initialisation of fgPalette
   new TStyle("Plain",  "Plain Style (no colors/fill areas)");
   new TStyle("Bold",   "Bold Style");
   new TStyle("Video",  "Style for video presentation histograms");
   new TStyle("Pub",    "Style for Publications");
   new TStyle("Classic","Classic Style");
   new TStyle("Default","Equivalent to Classic");
   new TStyle("Modern", "Modern Style");
   new TStyle("ATLAS",  "ATLAS Style");
   new TStyle("BELLE2", "Belle II Style");
   delete col;
}

////////////////////////////////////////////////////////////////////////////////
/// Change current style.

void TStyle::cd()
{
   gStyle = this;
}

////////////////////////////////////////////////////////////////////////////////
/// Copy this style.

void TStyle::Copy(TObject &obj) const
{
   TAttLine::Copy(((TStyle&)obj));
   TAttFill::Copy(((TStyle&)obj));
   TAttMarker::Copy(((TStyle&)obj));
   TAttText::Copy(((TStyle&)obj));
   fXaxis.Copy(((TStyle&)obj).fXaxis);
   fYaxis.Copy(((TStyle&)obj).fYaxis);
   fZaxis.Copy(((TStyle&)obj).fZaxis);
   fAttDate.Copy(((TStyle&)obj).fAttDate);
   ((TStyle&)obj).fIsReading        = fIsReading;
   ((TStyle&)obj).fScreenFactor     = fScreenFactor;
   ((TStyle&)obj).fCanvasPreferGL   = fCanvasPreferGL;
   ((TStyle&)obj).fCanvasColor      = fCanvasColor;
   ((TStyle&)obj).fCanvasBorderSize = fCanvasBorderSize;
   ((TStyle&)obj).fCanvasBorderMode = fCanvasBorderMode;
   ((TStyle&)obj).fCanvasDefH       = fCanvasDefH;
   ((TStyle&)obj).fCanvasDefW       = fCanvasDefW;
   ((TStyle&)obj).fCanvasDefX       = fCanvasDefX;
   ((TStyle&)obj).fCanvasDefY       = fCanvasDefY;
   ((TStyle&)obj).fPadColor         = fPadColor;
   ((TStyle&)obj).fPadBorderSize    = fPadBorderSize;
   ((TStyle&)obj).fPadBorderMode    = fPadBorderMode;
   ((TStyle&)obj).fPadBottomMargin  = fPadBottomMargin;
   ((TStyle&)obj).fPadTopMargin     = fPadTopMargin;
   ((TStyle&)obj).fPadLeftMargin    = fPadLeftMargin;
   ((TStyle&)obj).fPadRightMargin   = fPadRightMargin;
   ((TStyle&)obj).fPadGridX         = fPadGridX;
   ((TStyle&)obj).fPadGridY         = fPadGridY;
   ((TStyle&)obj).fPadTickX         = fPadTickX;
   ((TStyle&)obj).fPadTickY         = fPadTickY;
   ((TStyle&)obj).fPaperSizeX       = fPaperSizeX;
   ((TStyle&)obj).fPaperSizeY       = fPaperSizeY;
   ((TStyle&)obj).fFuncColor        = fFuncColor;
   ((TStyle&)obj).fFuncStyle        = fFuncStyle;
   ((TStyle&)obj).fFuncWidth        = fFuncWidth;
   ((TStyle&)obj).fGridColor        = fGridColor;
   ((TStyle&)obj).fGridStyle        = fGridStyle;
   ((TStyle&)obj).fGridWidth        = fGridWidth;
   ((TStyle&)obj).fHatchesSpacing   = fHatchesSpacing;
   ((TStyle&)obj).fHatchesLineWidth = fHatchesLineWidth;
   ((TStyle&)obj).fFrameFillColor   = fFrameFillColor;
   ((TStyle&)obj).fFrameFillStyle   = fFrameFillStyle;
   ((TStyle&)obj).fFrameLineColor   = fFrameLineColor;
   ((TStyle&)obj).fFrameLineStyle   = fFrameLineStyle;
   ((TStyle&)obj).fFrameLineWidth   = fFrameLineWidth;
   ((TStyle&)obj).fFrameBorderSize  = fFrameBorderSize;
   ((TStyle&)obj).fFrameBorderMode  = fFrameBorderMode;
   ((TStyle&)obj).fHistFillColor    = fHistFillColor;
   ((TStyle&)obj).fHistFillStyle    = fHistFillStyle;
   ((TStyle&)obj).fHistLineColor    = fHistLineColor;
   ((TStyle&)obj).fHistLineStyle    = fHistLineStyle;
   ((TStyle&)obj).fHistLineWidth    = fHistLineWidth;
   ((TStyle&)obj).fHistMinimumZero  = fHistMinimumZero;
   ((TStyle&)obj).fHistTopMargin    = fHistTopMargin;
   ((TStyle&)obj).fBarWidth         = fBarWidth;
   ((TStyle&)obj).fBarOffset        = fBarOffset;
   ((TStyle&)obj).fDrawBorder       = fDrawBorder;
   ((TStyle&)obj).fOptLogx          = fOptLogx;
   ((TStyle&)obj).fOptLogy          = fOptLogy;
   ((TStyle&)obj).fOptLogz          = fOptLogz;
   ((TStyle&)obj).fOptDate          = fOptDate;
   ((TStyle&)obj).fOptFile          = fOptFile;
   ((TStyle&)obj).fOptFit           = fOptFit;
   ((TStyle&)obj).fOptStat          = fOptStat;
   ((TStyle&)obj).fOptTitle         = fOptTitle;
   ((TStyle&)obj).fEndErrorSize     = fEndErrorSize;
   ((TStyle&)obj).fErrorX           = fErrorX;
   ((TStyle&)obj).fStatColor        = fStatColor;
   ((TStyle&)obj).fStatTextColor    = fStatTextColor;
   ((TStyle&)obj).fStatBorderSize   = fStatBorderSize;
   ((TStyle&)obj).fStatFont         = fStatFont;
   ((TStyle&)obj).fStatFontSize     = fStatFontSize;
   ((TStyle&)obj).fStatStyle        = fStatStyle;
   ((TStyle&)obj).fStatFormat       = fStatFormat;
   ((TStyle&)obj).fStatW            = fStatW;
   ((TStyle&)obj).fStatH            = fStatH ;
   ((TStyle&)obj).fStatX            = fStatX;
   ((TStyle&)obj).fStatY            = fStatY;
   ((TStyle&)obj).fTitleAlign       = fTitleAlign;
   ((TStyle&)obj).fTitleColor       = fTitleColor;
   ((TStyle&)obj).fTitleTextColor   = fTitleTextColor;
   ((TStyle&)obj).fTitleFont        = fTitleFont;
   ((TStyle&)obj).fTitleFontSize    = fTitleFontSize;
   ((TStyle&)obj).fTitleStyle       = fTitleStyle;
   ((TStyle&)obj).fTitleBorderSize  = fTitleBorderSize;
   ((TStyle&)obj).fTitleW           = fTitleW;
   ((TStyle&)obj).fTitleH           = fTitleH;
   ((TStyle&)obj).fTitleX           = fTitleX;
   ((TStyle&)obj).fTitleY           = fTitleY;
   ((TStyle&)obj).fDateX            = fDateX;
   ((TStyle&)obj).fDateY            = fDateY;
   ((TStyle&)obj).fFitFormat        = fFitFormat;
   ((TStyle&)obj).fPaintTextFormat  = fPaintTextFormat;
   ((TStyle&)obj).fShowEventStatus  = fShowEventStatus;
   ((TStyle&)obj).fShowEditor       = fShowEditor;
   ((TStyle&)obj).fShowToolBar      = fShowToolBar;
   ((TStyle&)obj).fLegoInnerR       = fLegoInnerR;
   ((TStyle&)obj).fStripDecimals    = fStripDecimals;
   ((TStyle&)obj).fNumberContours   = fNumberContours;
   ((TStyle&)obj).fLegendBorderSize = fLegendBorderSize;
   ((TStyle&)obj).fLegendFillColor  = fLegendFillColor;
   ((TStyle&)obj).fLegendFillStyle  = fLegendFillStyle;
   ((TStyle&)obj).fLegendFont       = fLegendFont;
   ((TStyle&)obj).fLegendTextSize   = fLegendTextSize;

   for (Int_t i=0;i<30;i++)
      ((TStyle&)obj).fLineStyle[i]     = fLineStyle[i];

   ((TStyle&)obj).fHeaderPS       = fHeaderPS;
   ((TStyle&)obj).fTitlePS        = fTitlePS;
   ((TStyle&)obj).fLineScalePS    = fLineScalePS;
   ((TStyle&)obj).fJoinLinePS     = fJoinLinePS;
   ((TStyle&)obj).fCapLinePS      = fCapLinePS;
   ((TStyle&)obj).fColorModelPS   = fColorModelPS;
   ((TStyle&)obj).fTimeOffset     = fTimeOffset;
   ((TStyle&)obj).fImageScaling   = fImageScaling;

   ((TStyle&)obj).fCandleWhiskerRange = fCandleWhiskerRange;
   ((TStyle&)obj).fCandleBoxRange     = fCandleBoxRange;
   ((TStyle&)obj).fCandleScaled       = fCandleScaled;
   ((TStyle&)obj).fViolinScaled       = fViolinScaled;

   ((TStyle&)obj).fOrthoCamera        = fOrthoCamera;

   ((TStyle&)obj).fXAxisExpXOffset    = fXAxisExpXOffset;
   ((TStyle&)obj).fXAxisExpYOffset    = fXAxisExpYOffset;
   ((TStyle&)obj).fYAxisExpXOffset    = fYAxisExpXOffset;
   ((TStyle&)obj).fYAxisExpYOffset    = fYAxisExpYOffset;
   ((TStyle&)obj).fAxisMaxDigits      = fAxisMaxDigits;
}

////////////////////////////////////////////////////////////////////////////////
/// Function used by the TStyle manager when drawing a canvas showing the
/// current style.

Int_t TStyle::DistancetoPrimitive(Int_t /*px*/, Int_t /*py*/)
{
   gPad->SetSelected(this);
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Reset.

void TStyle::Reset(Option_t *opt)
{
   fIsReading = kTRUE;
   TAttLine::ResetAttLine();
   TAttFill::ResetAttFill();
   TAttText::ResetAttText();
   TAttMarker::ResetAttMarker();
   SetFillStyle(1001);
   SetFillColor(19);
   fXaxis.ResetAttAxis("X");
   fYaxis.ResetAttAxis("Y");
   fZaxis.ResetAttAxis("Z");
   if (gEnv) fCanvasPreferGL = gEnv->GetValue("OpenGL.CanvasPreferGL",0);
   else      fCanvasPreferGL = kFALSE;
   fCanvasColor    = 19;
   fCanvasBorderSize= 2;
   fCanvasBorderMode= 1;
   fCanvasDefH     = 500;
   fCanvasDefW     = 700;
   fCanvasDefX     = 10;
   fCanvasDefY     = 10;
   fPadColor       = fCanvasColor;
   fPadBorderSize  = fCanvasBorderSize;
   fPadBorderMode  = fCanvasBorderMode;
   fPadBottomMargin= 0.1;
   fPadTopMargin   = 0.1;
   fPadLeftMargin  = 0.1;
   fPadRightMargin = 0.1;
   fPadGridX       = kFALSE;
   fPadGridY       = kFALSE;
   fPadTickX       = 0;
   fPadTickY       = 0;
   fFuncColor      = 1;
   fFuncStyle      = 1;
   fFuncWidth      = 3;
   fGridColor      = 0;
   fGridStyle      = 3;
   fGridWidth      = 1;
   fHatchesSpacing = 1;
   fHatchesLineWidth = 1;
   fHistLineColor  = 1;
   fHistFillColor  = 0;
   fHistFillStyle  = 1001;
   fHistLineStyle  = 1;
   fHistLineWidth  = 1;
   fHistMinimumZero= kFALSE;
   fHistTopMargin  = 0.05;
   fFrameLineColor = 1;
   fFrameFillColor = 0;
   fFrameFillStyle = 1001;
   fFrameLineStyle = 1;
   fFrameLineWidth = 1;
   fFrameBorderSize= 1;
   fFrameBorderMode= 1;
   fBarWidth       = 1;
   fBarOffset      = 0;
   fCandleCircleLineWidth = 1;
   fCandleCrossLineWidth = 1;
   fDrawBorder     = 0;
   fOptLogx        = 0;
   fOptLogy        = 0;
   fOptLogz        = 0;
   fOptDate        = 0;
   fOptFile        = 0;
   fOptFit         = 0;
   fOptStat        = 1;
   fOptTitle       = 1;
   fEndErrorSize   = 2;
   fErrorX         = 0.5;
   fScreenFactor   = 1;
   fStatColor      = fCanvasColor;
   fStatTextColor  = 1;
   fStatBorderSize = 2;
   fStatFont       = 62;
   fStatFontSize   = 0;
   fStatStyle      = 1001;
   fStatW          = 0.20;
   fStatH          = 0.16;
   fStatX          = 0.98;
   fStatY          = 0.995;
   SetStatFormat();
   SetFitFormat();
   SetPaintTextFormat();
   fTitleAlign     = 13;
   fTitleColor     = fCanvasColor;
   fTitleTextColor = 1;
   fTitleFont      = 62;
   fTitleFontSize  = 0;
   fTitleStyle     = 1001;
   fTitleBorderSize= 2;
   fTitleW         = 0;
   fTitleH         = 0;
   fTitleX         = 0.01;
   fTitleY         = 0.995;
   fShowEventStatus= 0;
   fShowEditor     = 0;
   fShowToolBar    = 0;
   fLegoInnerR     = 0.5;
   fHeaderPS       = "";
   fTitlePS        = "";
   fStripDecimals  = kTRUE;
   fNumberContours = 20;
   fLegendBorderSize= 4;
   fLegendFont      = 62;
   fLegendTextSize  = 0.,
   fLegendFillColor = 0;
   fLegendFillStyle = 1001;
   fImageScaling    = 1.;

   SetDateX();
   SetDateY();
   fAttDate.SetTextSize(0.025);
   fAttDate.SetTextAlign(11);
   SetLineScalePS();
   SetJoinLinePS();
   SetCapLinePS();
   SetColorModelPS();
   SetLineStyleString(1," ");
   SetLineStyleString(2,"12 12");
   SetLineStyleString(3,"4 8");
   SetLineStyleString(4,"12 16 4 16");
   SetLineStyleString(5,"20 12 4 12");
   SetLineStyleString(6,"20 12 4 12 4 12 4 12");
   SetLineStyleString(7,"20 20");
   SetLineStyleString(8,"20 12 4 12 4 12");
   SetLineStyleString(9,"80 20");
   SetLineStyleString(10,"80 40 4 40");
   for (Int_t i=11;i<30;i++) SetLineStyleString(i," ");

   SetPaperSize();

   SetPalette();

   fTimeOffset = 788918400; // UTC time at 01/01/95

   fCandleWhiskerRange  = 1.0;
   fCandleBoxRange      = 0.5;
   fCandleScaled = kFALSE;
   fViolinScaled = kTRUE;

   fOrthoCamera = kFALSE;

   fXAxisExpXOffset = 0;
   fXAxisExpYOffset = 0;
   fYAxisExpXOffset = 0;
   fYAxisExpYOffset = 0;
   fAxisMaxDigits = 5;

   TString style_name = opt;

   if (strcmp(style_name,"Modern") == 0) {
      // Modern style
      SetFrameBorderMode(0);
      SetFrameFillColor(0);
      SetCanvasBorderMode(0);
      SetCanvasColor(0);
      SetPadBorderMode(0);
      SetPadColor(0);
      SetStatColor(0);
      SetTitleFont(42,"");
      SetLabelFont(42,"x");
      SetTitleFont(42,"x");
      SetLabelFont(42,"y");
      SetTitleFont(42,"y");
      SetLabelFont(42,"z");
      SetTitleFont(42,"z");
      SetStatFont(42);
      SetLabelSize(0.035,"x");
      SetTitleSize(0.035,"x");
      SetLabelSize(0.035,"y");
      SetTitleSize(0.035,"y");
      SetLabelSize(0.035,"z");
      SetTitleSize(0.035,"z");
      SetTitleSize(0.050,"");
      SetTitleAlign(23);
      SetTitleX(0.5);
      SetTitleBorderSize(0);
      SetTitleFillColor(0);
      SetTitleStyle(0);
      SetTitleOffset(0.,"Y");
      SetStatBorderSize(1);
      SetOptStat(1111);
      SetStatY(0.935);
      SetHistLineColor(kBlue+2);
      SetLegendBorderSize(1);
      SetLegendFillColor(0);
      SetLegendFillStyle(1001);
      SetLegendFont(42);
      SetLegendTextSize(0.);
      SetFuncWidth(2);
      SetFuncColor(2);
   }
   if (strcmp(style_name,"Plain") == 0) {
      SetFrameBorderMode(0);
      SetCanvasBorderMode(0);
      SetPadBorderMode(0);
      SetPadColor(0);
      SetCanvasColor(0);
      SetTitleFillColor(0);
      SetTitleBorderSize(1);
      SetStatColor(0);
      SetStatBorderSize(1);
      SetLegendBorderSize(1);
      return;
   }
   if (strcmp(style_name,"Bold") == 0) {
      SetPalette(1,nullptr);
      SetCanvasColor(10);
      SetCanvasBorderMode(0);
      SetFrameLineWidth(3);
      SetFrameFillColor(10);
      SetPadColor(10);
      SetPadTickX(1);
      SetPadTickY(1);
      SetPadBottomMargin(0.15);
      SetPadLeftMargin(0.15);
      SetHistLineWidth(3);
      SetHistLineColor(kRed);
      SetFuncWidth(3);
      SetFuncColor(kGreen);
      SetLineWidth(3);
      SetLabelSize(0.05,"xyz");
      SetLabelOffset(0.01,"y");
      SetLabelColor(kBlue,"xy");
      SetTitleSize(0.06,"xyz");
      SetTitleOffset(1.3,"Y");
      SetTitleFillColor(10);
      SetTitleTextColor(kBlue);
      SetStatColor(10);
      return;
   }
   if (strcmp(style_name,"Video") == 0) {
      SetPalette(1,nullptr);
      SetCanvasColor(10);
      SetCanvasBorderMode(0);
      SetFrameLineWidth(3);
      SetFrameFillColor(10);
      SetPadColor(10);
      SetPadTickX(1);
      SetPadTickY(1);
      SetPadBottomMargin(0.2);
      SetPadLeftMargin(0.2);
      SetHistLineWidth(8);
      SetHistLineColor(kRed);
      SetLabelSize(0.06,"xyz");
      SetLabelColor(kBlue,"xyz");
      SetTitleSize(0.08,"xyz");
      SetTitleFillColor(10);
      SetTitleTextColor(kBlue);
      SetStatColor(10);
      SetFuncWidth(8);
      SetFuncColor(kGreen);
      SetLineWidth(3);
      return;
   }
   if (strcmp(style_name,"Pub") == 0) {
      SetOptTitle(0);
      SetOptStat(0);
      SetPalette(8,nullptr);
      SetCanvasColor(10);
      SetCanvasBorderMode(0);
      SetFrameLineWidth(3);
      SetFrameFillColor(10);
      SetPadColor(10);
      SetPadTickX(1);
      SetPadTickY(1);
      SetPadBottomMargin(0.15);
      SetPadLeftMargin(0.15);
      SetHistLineWidth(3);
      SetHistLineColor(kRed);
      SetFuncWidth(3);
      SetFuncColor(kGreen);
      SetLineWidth(3);
      SetLabelSize(0.05,"xyz");
      SetLabelOffset(0.01,"y");
      SetLabelColor(kBlack,"xyz");
      SetTitleSize(0.06,"xyz");
      SetTitleOffset(1.3,"y");
      SetTitleFillColor(10);
      SetTitleTextColor(kBlue);
      return;
   }
   if (strcmp(style_name,"ATLAS") == 0) {
      SetFrameBorderMode(0);
      SetFrameFillColor(0);
      SetCanvasBorderMode(0);
      SetCanvasColor(0);
      SetPadBorderMode(0);
      SetPadColor(0);
      SetStatColor(0);
      SetPaperSize(20,26);
      SetPadTopMargin(0.05);
      SetPadRightMargin(0.05);
      SetPadBottomMargin(0.16);
      SetPadLeftMargin(0.16);
      SetTitleXOffset(1.4);
      SetTitleYOffset(1.4);
      Int_t font = 42;
      Double_t tsize=0.05;
      SetTextFont(font);
      SetTextSize(tsize);
      SetLabelFont(font,"x");
      SetTitleFont(font,"x");
      SetLabelFont(font,"y");
      SetTitleFont(font,"y");
      SetLabelFont(font,"z");
      SetTitleFont(font,"z");
      SetLabelSize(tsize,"x");
      SetTitleSize(tsize,"x");
      SetLabelSize(tsize,"y");
      SetTitleSize(tsize,"y");
      SetLabelSize(tsize,"z");
      SetTitleSize(tsize,"z");
      SetMarkerStyle(20);
      SetMarkerSize(1.2);
      SetHistLineWidth(2.);
      SetLineStyleString(2,"[12 12]");
      SetEndErrorSize(0.);
      SetOptTitle(0);
      SetOptStat(0);
      SetOptFit(0);
      SetPadTickX(1);
      SetPadTickY(1);
      return;
   }
   if (strcmp(style_name,"BELLE2") == 0) {
      Int_t icol=0;
      SetFrameBorderMode(icol);
      SetFrameFillColor(icol);
      SetCanvasBorderMode(icol);
      SetCanvasColor(icol);
      SetPadBorderMode(icol);
      SetPadColor(icol);
      SetStatColor(icol);
      SetPaperSize(20,26);
      SetPadTopMargin(0.05);
      SetPadRightMargin(0.05);
      SetPadBottomMargin(0.16);
      SetPadLeftMargin(0.16);
      SetTitleXOffset(1.0);
      SetTitleYOffset(1.0);
      Int_t font=42;
      Double_t tsize=0.05;
      SetTextFont(font);
      SetTextSize(tsize);
      SetLabelFont(font,"x");
      SetTitleFont(font,"x");
      SetLabelFont(font,"y");
      SetTitleFont(font,"y");
      SetLabelFont(font,"z");
      SetTitleFont(font,"z");
      SetLabelSize(tsize,"x");
      SetTitleSize(.065,"x");
      SetLabelSize(tsize,"y");
      SetTitleSize(.065,"y");
      SetLabelSize(tsize,"z");
      SetTitleSize(.065,"z");
      SetTitleOffset(1.1,"x");
      SetTitleOffset(1.1,"y");
      SetTitleOffset(1.1,"z");
      SetLabelOffset(0.015,"x");
      SetLabelOffset(0.015,"y");
      SetLabelOffset(0.015,"z");
      SetTickLength(0.03,"x");
      SetTickLength(0.02,"y");
      SetMarkerStyle(20);
      SetMarkerSize(0.9);
      SetHistLineWidth(2);
      SetLineStyleString(2,"[12 12]");
      SetErrorX(0.001);
      SetEndErrorSize(0.);
      SetOptTitle(0);
      SetOptStat(0);
      SetOptFit(0);
      SetPadTickX(0);
      SetPadTickY(0);
      SetFrameLineWidth(2);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Return number of divisions.

Int_t TStyle::GetNdivisions(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetNdivisions();
   if (ax == 2) return fYaxis.GetNdivisions();
   if (ax == 3) return fZaxis.GetNdivisions();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return the axis color number in the axis.

Color_t TStyle::GetAxisColor(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetAxisColor();
   if (ax == 2) return fYaxis.GetAxisColor();
   if (ax == 3) return fZaxis.GetAxisColor();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return color number i in current palette.

Int_t TStyle::GetColorPalette(Int_t i) const
{
   return TColor::GetColorPalette(i);
}

////////////////////////////////////////////////////////////////////////////////
/// Return the label color number in the axis.

Color_t TStyle::GetLabelColor(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetLabelColor();
   if (ax == 2) return fYaxis.GetLabelColor();
   if (ax == 3) return fZaxis.GetLabelColor();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return label font.

Style_t TStyle::GetLabelFont(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetLabelFont();
   if (ax == 2) return fYaxis.GetLabelFont();
   if (ax == 3) return fZaxis.GetLabelFont();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return label offset.

Float_t TStyle::GetLabelOffset(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetLabelOffset();
   if (ax == 2) return fYaxis.GetLabelOffset();
   if (ax == 3) return fZaxis.GetLabelOffset();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return label size.

Float_t TStyle::GetLabelSize(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetLabelSize();
   if (ax == 2) return fYaxis.GetLabelSize();
   if (ax == 3) return fZaxis.GetLabelSize();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Method returns maximum number of digits permitted for the axis labels above which the
/// notation with 10^N is used. See @ref SetAxisMaxDigits for more details
Int_t TStyle::GetAxisMaxDigits() const
{
   return fAxisMaxDigits;
}

////////////////////////////////////////////////////////////////////////////////
/// Return line style string (used by PostScript).
/// See SetLineStyleString for more explanations

const char *TStyle::GetLineStyleString(Int_t i) const
{
   if (i < 1 || i > 29) return fLineStyle[0].Data();
   return fLineStyle[i].Data();
}

////////////////////////////////////////////////////////////////////////////////
/// Return number of colors in the color palette.

Int_t TStyle::GetNumberOfColors() const
{
   return TColor::GetNumberOfColors();
}

////////////////////////////////////////////////////////////////////////////////
/// Set paper size for PostScript output.

void TStyle::GetPaperSize(Float_t &xsize, Float_t &ysize) const
{
   xsize = fPaperSizeX;
   ysize = fPaperSizeY;
}

////////////////////////////////////////////////////////////////////////////////
/// Return tick length.

Float_t TStyle::GetTickLength(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetTickLength();
   if (ax == 2) return fYaxis.GetTickLength();
   if (ax == 3) return fZaxis.GetTickLength();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return title color.

Color_t TStyle::GetTitleColor(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetTitleColor();
   if (ax == 2) return fYaxis.GetTitleColor();
   if (ax == 3) return fZaxis.GetTitleColor();
   return fTitleTextColor;
}

////////////////////////////////////////////////////////////////////////////////
/// Return title font.

Style_t TStyle::GetTitleFont(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetTitleFont();
   if (ax == 2) return fYaxis.GetTitleFont();
   if (ax == 3) return fZaxis.GetTitleFont();
   return fTitleFont;
}

////////////////////////////////////////////////////////////////////////////////
/// Return title offset.

Float_t TStyle::GetTitleOffset(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetTitleOffset();
   if (ax == 2) return fYaxis.GetTitleOffset();
   if (ax == 3) return fZaxis.GetTitleOffset();
   return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Return title size.

Float_t TStyle::GetTitleSize(Option_t *axis) const
{
   Int_t ax = AxisChoice(axis);
   if (ax == 1) return fXaxis.GetTitleSize();
   if (ax == 2) return fYaxis.GetTitleSize();
   if (ax == 3) return fZaxis.GetTitleSize();
   return fTitleFontSize;
}

////////////////////////////////////////////////////////////////////////////////
/// Copy this style to gStyle.

void TStyle::Paint(Option_t *)
{
   Copy(*gStyle);
}

////////////////////////////////////////////////////////////////////////////////
/// Define the color model used by TPostScript and TPDF (RGB or CMYK).
/// CMY and CMYK models are subtractive color models unlike RGB which is
/// additive. They are mainly used for printing purposes. CMY means Cyan Magenta
/// Yellow. To convert RGB to CMY it is enough to do: C=1-R, M=1-G and Y=1-B.
/// CMYK has one more component K (black). The conversion from RGB to CMYK is:
/// ~~~ {.cpp}
/// Double_t Black   = TMath::Min(TMath::Min(1-Red,1-Green),1-Blue);
/// Double_t Cyan    = (1-Red-Black)/(1-Black);
/// Double_t Magenta = (1-Green-Black)/(1-Black);
/// Double_t Yellow  = (1-Blue-Black)/(1-Black);
/// ~~~
/// CMYK adds the black component which allows better quality for black
/// printing. PostScript and PDF support the CMYK model.
///
///  - c = 0 means TPostScript and TPDF will use RGB color model (default)
///  - c = 1 means TPostScript and TPDF will use CMYK color model

void TStyle::SetColorModelPS(Int_t c)
{
   fColorModelPS = c;
}

////////////////////////////////////////////////////////////////////////////////
/// If the argument zero=kTRUE the minimum value for the Y axis of 1-d histograms
/// is set to 0.
///
/// If the minimum bin content is greater than 0 and TH1::SetMinimum
/// has not been called.
/// Otherwise the minimum is based on the minimum bin content.

void TStyle::SetHistMinimumZero(Bool_t zero)
{
   fHistMinimumZero = zero;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the number of divisions to draw an axis.
///  ndiv      : Number of divisions.
/// ~~~ {.cpp}
///       n = N1 + 100*N2 + 10000*N3
///       N1=number of primary divisions.
///       N2=number of secondary divisions.
///       N3=number of 3rd divisions.
///           e.g.:
///           nndi=0 --> no tick marks.
///           nndi=2 --> 2 divisions, one tick mark in the middle
///                      of the axis.
/// ~~~
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetNdivisions(Int_t n, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();
   if (opt.Contains("x")) fXaxis.SetNdivisions(n);
   if (opt.Contains("y")) fYaxis.SetNdivisions(n);
   if (opt.Contains("z")) fZaxis.SetNdivisions(n);
}

////////////////////////////////////////////////////////////////////////////////
/// Set color to draw the axis line and tick marks.
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetAxisColor(Color_t color, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) fXaxis.SetAxisColor(color);
   if (opt.Contains("y")) fYaxis.SetAxisColor(color);
   if (opt.Contains("z")) fZaxis.SetAxisColor(color);
}

////////////////////////////////////////////////////////////////////////////////
/// Set the size (in pixels) of the small lines drawn at the
/// end of the error bars (TH1 or TGraphErrors).
///
/// The default value is 2 pixels.
/// Set np=0 to remove these lines

void TStyle::SetEndErrorSize(Float_t np)
{
   if (np >= 0) fEndErrorSize = np;
   else         fEndErrorSize = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// Define a string to be inserted in the Postscript header.
///
/// The string in header will be added to the Postscript file
/// immediately following the %%Page line
/// For example, this string may contain special Postscript instructions like
/// ~~~ {.cpp}
///      200 200 translate
/// ~~~
/// the following header string will print the string "my annotation" at the
/// bottom left corner of the page (outside the user area)
/// ~~~ {.cpp}
///  "gsave 100 -100 t 0 r 0 0 m /Helvetica-Bold findfont 56 sf 0 0 m ( my annotation ) show gr"
/// ~~~
/// This information is used in TPostScript::Initialize

void TStyle::SetHeaderPS(const char *header)
{
   fHeaderPS = header;
}

////////////////////////////////////////////////////////////////////////////////
/// Sets the `fIsReading` member to reading (default=kTRUE).
///
/// `fIsReading` (used via `gStyle->IsReading()`) can be used in
/// the functions `myclass::UseCurrentStyle` to read from the current style
/// or write to the current style

void TStyle::SetIsReading(Bool_t reading)
{
   fIsReading = reading;
}

////////////////////////////////////////////////////////////////////////////////
/// Define a string to be used in the %%Title of the Postscript files.
/// If this string is not defined, ROOT will use the canvas title.

void TStyle::SetTitlePS(const char *pstitle)
{
   fTitlePS = pstitle;
}

////////////////////////////////////////////////////////////////////////////////
/// Set axis labels color.
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetLabelColor(Color_t color, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) fXaxis.SetLabelColor(color);
   if (opt.Contains("y")) fYaxis.SetLabelColor(color);
   if (opt.Contains("z")) fZaxis.SetLabelColor(color);
}

////////////////////////////////////////////////////////////////////////////////
/// Set font number used to draw axis labels.
///  - font  :  Text font code = 10*fontnumber + precision
///           - Font numbers must be between 1 and 14
///           - precision = 1 fast hardware fonts (steps in the size)
///           - precision = 2 scalable and rotatable hardware fonts
/// The default font number is 62.
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetLabelFont(Style_t font, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) fXaxis.SetLabelFont(font);
   if (opt.Contains("y")) fYaxis.SetLabelFont(font);
   if (opt.Contains("z")) fZaxis.SetLabelFont(font);
}

////////////////////////////////////////////////////////////////////////////////
/// Set offset between axis and axis labels.
/// The offset is expressed as a percent of the pad height.
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetLabelOffset(Float_t offset, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) fXaxis.SetLabelOffset(offset);
   if (opt.Contains("y")) fYaxis.SetLabelOffset(offset);
   if (opt.Contains("z")) fZaxis.SetLabelOffset(offset);
}

////////////////////////////////////////////////////////////////////////////////
/// Set size of axis labels. The size is expressed as a percent of the pad height.
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetLabelSize(Float_t size, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) fXaxis.SetLabelSize(size);
   if (opt.Contains("y")) fYaxis.SetLabelSize(size);
   if (opt.Contains("z")) fZaxis.SetLabelSize(size);
}

////////////////////////////////////////////////////////////////////////////////
/// Set line style string using the PostScript convention.
/// A line is a suite of segments, each segment is described by the number of
/// pixels. The initial and alternating elements (second, fourth, and so on)
/// are the dashes, and the others spaces between dashes.
///
/// Default fixed line styles are pre-defined as:
/// ~~~ {.cpp}
///   linestyle 1  "[]"             solid
///   linestyle 2  "[12 12]"        dashed
///   linestyle 3  "[4 8]"          dotted
///   linestyle 4  "[12 16 4 16]"   dash-dotted
/// ~~~
///  For example the following lines define the line style 5 to 9.
/// ~~~ {.cpp}
///   gStyle->SetLineStyleString(5,"20 12 4 12");
///   gStyle->SetLineStyleString(6,"20 12 4 12 4 12 4 12");
///   gStyle->SetLineStyleString(7,"20 20");
///   gStyle->SetLineStyleString(8,"20 12 4 12 4 12");
///   gStyle->SetLineStyleString(9,"80 20");
/// ~~~
/// \image html base_linestyle.png
/// Note:
///  - Up to 30 different styles may be defined.
///  - The opening and closing brackets may be omitted
///  - It is recommended to use 4 as the smallest segment length and multiple of
///    4 for other lengths.
///  - The line style 1 to 10 are predefined. 1 to 4 cannot be changed.

void TStyle::SetLineStyleString(Int_t i, const char *text)
{
   if (!text) text = "";
   char *l;
   Int_t nch = strlen(text);
   char *st = new char[nch+10];
   snprintf(st,nch+10," ");
   strlcat(st,text,nch+10);
   l = strstr(st,"["); if (l) l[0] = ' ';
   l = strstr(st,"]"); if (l) l[0] = ' ';
   if (i >= 1 && i <= 29) fLineStyle[i] = st;
   delete [] st;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the default number of contour levels when drawing 2-d plots.

void TStyle::SetNumberContours(Int_t number)
{
   if (number > 0 && number < 1000) {
      fNumberContours = number;
      return;
   }

   Error("SetNumberContours","Illegal number of contours: %d, must be > 0 and < 1000",number);
}

////////////////////////////////////////////////////////////////////////////////
/// If optdate is non null, the current date/time will be printed in the canvas.
/// The position of the date string can be controlled by:
///  optdate = 10*format + mode
///  -  mode = 1   (default) date is printed in the bottom/left corner.
///  -  mode = 2   date is printed in the bottom/right corner.
///  -  mode = 3   date is printed in the top/right corner.
///  -  format = 0 (default) date has the format like: "Wed Sep 25 17:10:35 2002"
///  -  format = 1 date has the format like: "2002-09-25"
///  -  format = 2 date has the format like: "2002-09-25 17:10:35"
///
///  examples:
///  -  optdate = 1  date like "Wed Sep 25 17:10:35 2002" in the bottom/left corner.
///  -  optdate = 13 date like "2002-09-25" in the top/right corner.
///
///  The date position can also be controlled by:
///    gStyle->SetDateX(x);  x in NDC
///    gStyle->SetDateY(y);  y in NDC
///
///  The date text attributes can be changed with:
/// ~~~ {.cpp}
///    gStyle->GetAttDate()->SetTextFont(font=62);
///    gStyle->GetAttDate()->SetTextSize(size=0.025);
///    gStyle->GetAttDate()->SetTextAngle(angle=0);
///    gStyle->GetAttDate()->SetTextAlign(align=11);
///    gStyle->GetAttDate()->SetTextColor(color=1);
/// ~~~
///  The current date attributes can be obtained via:
/// ~~~ {.cpp}
///    gStyle->GetAttDate()->GetTextxxxx();
/// ~~~
///  When the date option is active, a text object is created when the pad
///  paint its list of primitives. The text object is named "DATE".
///  The DATE attributes can also be edited interactively (position
///  and attributes) via the normal context menu.

void TStyle::SetOptDate(Int_t optdate)
{
   fOptDate = optdate;
   Int_t mode = optdate % 10;
   if (mode == 1) {
      SetDateX(0.01);
      SetDateY(0.01);
      fAttDate.SetTextAlign(11);
   }
   if (mode == 2) {
      SetDateX(0.99);
      SetDateY(0.01);
      fAttDate.SetTextAlign(31);
   }
   if (mode == 3) {
      SetDateX(0.99);
      SetDateY(0.99);
      fAttDate.SetTextAlign(33);
   }
}

////////////////////////////////////////////////////////////////////////////////
/// The type of information about fit parameters printed in the histogram
/// statistics box can be selected via the parameter `mode`.
///  The parameter mode can be = `pcev`:
///  -  p = 1;  print Probability
///  -  c = 1;  print Chisquare/Number of degrees of freedom
///  -  e = 1;  print errors (if e=1, v must be 1)
///  -  v = 1;  print name/values of parameters
///  Example: `gStyle->SetOptFit(1011);`
///           print fit probability, parameter names/values and errors.
///  -  When "v"=1 is specified, only the non-fixed parameters are shown.
///  -  When "v"=2 all parameters are shown.
///
///  #### Notes:
///
///  - never call `SetOptFit(000111);` but `SetOptFit(111)`, 000111 will
///    be taken as an octal number !!
///  - `gStyle->SetOptFit(1)` is a shortcut allowing to set the most common
///    case and is equivalent to `gStyle->SetOptFit(111)`
///  - At ROOT startup the option fit is set to `0`. So, to see the fit parameters
///    on all plot resulting from a fit, a call to `gStyle->SetOptFit()` with a
///    non null value should be done. One can put it in the `rootlogon.C` file to
///    always have it.
///
/// see also SetOptStat below.

void TStyle::SetOptFit(Int_t mode)
{
   fOptFit = mode;
   if (gPad) {
      TIter next(gPad->GetListOfPrimitives());
      while (auto obj = next()) {
         TObject *stats = obj->FindObject("stats");
         if (stats) stats->SetBit(kTakeStyle);
      }
      gPad->Modified(); gPad->Update();
   }
}

////////////////////////////////////////////////////////////////////////////////
/// The type of information printed in the histogram statistics box
///  can be selected via the parameter mode.
///  The parameter mode can be = `ksiourmen`
///  -  k = 1;  kurtosis printed
///  -  k = 2;  kurtosis and kurtosis error printed
///  -  s = 1;  skewness printed
///  -  s = 2;  skewness and skewness error printed
///  -  i = 1;  integral of bins printed
///  -  i = 2;  integral of bins with option "width" printed
///  -  o = 1;  number of overflows printed
///  -  u = 1;  number of underflows printed
///  -  r = 1;  rms printed
///  -  r = 2;  rms and rms error printed
///  -  m = 1;  mean value printed
///  -  m = 2;  mean and mean error values printed
///  -  e = 1;  number of entries printed
///  -  n = 1;  name of histogram is printed
///
///  Example: `gStyle->SetOptStat(11);`
///           print only name of histogram and number of entries.
///           `gStyle->SetOptStat(1101);`  displays the name of histogram, mean value and RMS.
///
///  #### Notes:
///
///  - never call `SetOptStat(000111);` but `SetOptStat(111)`, 000111 will
///    be taken as an octal number !!
///  - `SetOptStat(1)` is s shortcut allowing to set the most common case, and is
///    taken as `SetOptStat(1111)` (for backward compatibility with older versions.
///    If you want to print only the name of the histogram call `SetOptStat(1000000001)`.
///  - that in case of 2-D histograms, when selecting just underflow (10000)
///    or overflow (100000), the stats box will show all combinations
///    of underflow/overflows and not just one single number!

void TStyle::SetOptStat(Int_t mode)
{
   fOptStat = mode;
   if (gPad) {
      TIter next(gPad->GetListOfPrimitives());
      while (auto obj = next()) {
         TObject *stats = obj->FindObject("stats");
         if (stats) stats->SetBit(kTakeStyle);
      }
      gPad->Modified(); gPad->Update();
   }
}

////////////////////////////////////////////////////////////////////////////////
///  The parameter mode can be any combination of kKsSiourRmMen
///  -  k :  kurtosis printed
///  -  K :  kurtosis and kurtosis error printed
///  -  s :  skewness printed
///  -  S :  skewness and skewness error printed
///  -  i :  integral of bins printed
///  -  I :  integral of bins with option "width" printed
///  -  o :  number of overflows printed
///  -  u :  number of underflows printed
///  -  r :  rms printed
///  -  R :  rms and rms error printed
///  -  m :  mean value printed
///  -  M :  mean value mean error values printed
///  -  e :  number of entries printed
///  -  n :  name of histogram is printed
///
///  Example: `gStyle->SetOptStat("ne");`
///           print only name of histogram and number of entries.
///
///  - `gStyle->SetOptStat("n")` print only the name of the histogram
///  - `gStyle->SetOptStat("nemr")` is the default

void TStyle::SetOptStat(Option_t *stat)
{
   Int_t mode = 0;

   TString opt = stat;

   if (opt.Contains("n")) mode+=1;
   if (opt.Contains("e")) mode+=10;
   if (opt.Contains("m")) mode+=100;
   if (opt.Contains("M")) mode+=200;
   if (opt.Contains("r")) mode+=1000;
   if (opt.Contains("R")) mode+=2000;
   if (opt.Contains("u")) mode+=10000;
   if (opt.Contains("o")) mode+=100000;
   if (opt.Contains("i")) mode+=1000000;
   if (opt.Contains("I")) mode+=2000000;
   if (opt.Contains("s")) mode+=10000000;
   if (opt.Contains("S")) mode+=20000000;
   if (opt.Contains("k")) mode+=100000000;
   if (opt.Contains("K")) mode+=200000000;
   if (mode == 1) mode = 1000000001;

   SetOptStat(mode);
}

////////////////////////////////////////////////////////////////////////////////
/// Set paper size for PostScript output.

void TStyle::SetPaperSize(EPaperSize size)
{
   switch (size) {
      case kA4:
         SetPaperSize(20, 26);
         break;
      case kUSLetter:
         SetPaperSize(20, 24);
         break;
      default:
         Error("SetPaperSize", "illegal paper size %d\n", (int)size);
         break;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Set paper size for PostScript output.
/// The paper size is specified in centimeters. Default is 20x26.
/// See also TPad::Print

void TStyle::SetPaperSize(Float_t xsize, Float_t ysize)
{
   fPaperSizeX = xsize;
   fPaperSizeY = ysize;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the tick marks length for an axis.
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetTickLength(Float_t length, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) fXaxis.SetTickLength(length);
   if (opt.Contains("y")) fYaxis.SetTickLength(length);
   if (opt.Contains("z")) fZaxis.SetTickLength(length);
}

////////////////////////////////////////////////////////////////////////////////
///  - if axis =="x"  set the X axis title color
///  - if axis =="y"  set the Y axis title color
///  - if axis =="z"  set the Z axis title color
///
/// any other value of axis will set the pad title color
///
/// if axis="xyz" set all 3 axes

void TStyle::SetTitleColor(Color_t color, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   Bool_t set = kFALSE;
   if (opt.Contains("x")) { fXaxis.SetTitleColor(color); set = kTRUE; }
   if (opt.Contains("y")) { fYaxis.SetTitleColor(color); set = kTRUE; }
   if (opt.Contains("z")) { fZaxis.SetTitleColor(color); set = kTRUE; }
   if (!set) fTitleColor = color;
}

////////////////////////////////////////////////////////////////////////////////
///  - if axis =="x"  set the X axis title font
///  - if axis =="y"  set the Y axis title font
///  - if axis =="z"  set the Z axis title font
///
/// any other value of axis will set the pad title font
///
/// if axis="xyz" set all 3 axes

void TStyle::SetTitleFont(Style_t font, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   Bool_t set = kFALSE;
   if (opt.Contains("x")) { fXaxis.SetTitleFont(font); set = kTRUE; }
   if (opt.Contains("y")) { fYaxis.SetTitleFont(font); set = kTRUE; }
   if (opt.Contains("z")) { fZaxis.SetTitleFont(font); set = kTRUE; }
   if (!set) fTitleFont = font;
}

////////////////////////////////////////////////////////////////////////////////
/// Specify a parameter offset to control the distance between the axis
/// and the axis title.
///
///  - offset = 1 means : use the default distance
///  - offset = 1.2 means: the distance will be 1.2*(default distance)
///  - offset = 0.8 means: the distance will be 0.8*(default distance)
///
/// axis specifies which axis ("x","y","z"), default = "x"
/// if axis="xyz" set all 3 axes

void TStyle::SetTitleOffset(Float_t offset, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) fXaxis.SetTitleOffset(offset);
   if (opt.Contains("y")) fYaxis.SetTitleOffset(offset);
   if (opt.Contains("z")) fZaxis.SetTitleOffset(offset);
}

////////////////////////////////////////////////////////////////////////////////
///  - if axis =="x"  set the X axis title size
///  - if axis =="y"  set the Y axis title size
///  - if axis =="z"  set the Z axis title size
///
/// any other value of axis will set the pad title size
///
/// if axis="xyz" set all 3 axes

void TStyle::SetTitleSize(Float_t size, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   Bool_t set = kFALSE;
   if (opt.Contains("x")) { fXaxis.SetTitleSize(size); set = kTRUE; }
   if (opt.Contains("y")) { fYaxis.SetTitleSize(size); set = kTRUE; }
   if (opt.Contains("z")) { fZaxis.SetTitleSize(size); set = kTRUE; }
   if (!set) fTitleFontSize = size;
}

////////////////////////////////////////////////////////////////////////////////
/// Method set X and Y offset of the axis 10^n notation.
/// It applies on axis belonging to an histogram (TAxis). It has no effect on standalone TGaxis.
/// It is in % of the pad size. It can be negative.
/// axis specifies which axis ("x","y"), default = "x"
/// if axis="xz" set the two axes

void TStyle::SetExponentOffset(Float_t xoff, Float_t yoff, Option_t *axis)
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) {
      fXAxisExpXOffset = xoff;
      fXAxisExpYOffset = yoff;
   }
   if (opt.Contains("y")) {
      fYAxisExpXOffset = xoff;
      fYAxisExpYOffset = yoff;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Method returns X and Y offset of the axis 10^n notation.
/// It applies on axis belonging to an histogram (TAxis)

void TStyle::GetExponentOffset(Float_t &xoff, Float_t &yoff, Option_t *axis) const
{
   TString opt = axis;
   opt.ToLower();

   if (opt.Contains("x")) {
      xoff = fXAxisExpXOffset;
      yoff = fXAxisExpYOffset;
   } else if (opt.Contains("y")) {
      xoff = fYAxisExpXOffset;
      yoff = fYAxisExpYOffset;
   } else {
      xoff = yoff = 0.;
   }
}

////////////////////////////////////////////////////////////////////////////////
/// Method set maximum number of digits permitted for the axis labels above which the
/// notation with 10^N is used. For example, to accept 6 digits number like 900000
/// on an axis call `gStyle->SetAxisMaxDigits(6)`. The default value is 5.
/// Warning: this function changes the max number of digits in all axes.
/// If you only want to change the digits of the current TGaxis instance, use
/// axis->SetNdivisions(N*1000000 + (axis->GetNdiv()%1000000))
/// instead of gStyle->SetAxisMaxDigits(N).

void TStyle::SetAxisMaxDigits(Int_t maxd)
{
   fAxisMaxDigits = (maxd > 1) ? maxd : 1;
}

////////////////////////////////////////////////////////////////////////////////
/// See TColor::SetPalette.

void TStyle::SetPalette(Int_t ncolors, Int_t *colors, Float_t alpha)
{
   TColor::SetPalette(ncolors,colors,alpha);
}

////////////////////////////////////////////////////////////////////////////////
/// \see TColor::CreateColorTableFromFile, TColor::SetPalette
void TStyle::SetPalette(TString fileName, Float_t alpha)
{
   TColor::CreateColorTableFromFile(fileName, alpha);
}

////////////////////////////////////////////////////////////////////////////////
/// Change the time offset for time plotting.
/// Times are expressed in seconds. The corresponding numbers usually have 9
/// digits (or more if one takes into account fractions of seconds).
/// Thus, since it is very inconvenient to plot very large numbers on a scale,
/// one has to set an offset time that will be added to the axis beginning,
/// in order to plot times correctly and conveniently. A convenient way to
/// set the time offset is to use TDatime::Convert().
///
/// By default the time offset is set to 788918400 which corresponds to
/// 01/01/1995. This allows to have valid dates until 2072. The standard
/// UNIX time offset in 1970 allows only valid dates until 2030.

void TStyle::SetTimeOffset(Double_t toffset)
{
   fTimeOffset = toffset;
}

////////////////////////////////////////////////////////////////////////////////
///  Set option to strip decimals when drawing axis labels.
///  By default, TGaxis::PaintAxis removes trailing 0s after a dot
///  in the axis labels. Ex: {0,0.5,1,1.5,2,2.5, etc}
///  If this function is called with strip=kFALSE, TGAxis::PaintAxis will
///  draw labels with the same number of digits after the dot
///  Ex: (0.0,0.5,1.0,1.5,2.0,2.5,etc}

void TStyle::SetStripDecimals(Bool_t strip)
{
   fStripDecimals = strip;
}

////////////////////////////////////////////////////////////////////////////////
/// By setting whisker-range for candle plot, one can force
/// the whiskers to cover the fraction of the distribution.
/// Set wRange between 0 and 1. Default is 1
/// gStyle->SetCandleWhiskerRange(0.95) will set all candle-charts to cover 95% of
/// the distribution with the whiskers.
/// Can only be used with the standard-whisker definition

void TStyle::SetCandleWhiskerRange(Double_t wRange)
{
   if (wRange < 0)
      fCandleWhiskerRange = 0;
   else if (wRange > 1)
      fCandleWhiskerRange = 1;
   else
      fCandleWhiskerRange = wRange;
}

////////////////////////////////////////////////////////////////////////////////
/// By setting box-range for candle plot, one can force the
/// box of the candle-chart to cover that given fraction of the distribution.
/// Set bRange between 0 and 1. Default is 0.5
/// gStyle->SetCandleBoxRange(0.68) will set all candle-charts to cover 68% of the
/// distribution by the box

void TStyle::SetCandleBoxRange(Double_t bRange)
{
   if (bRange < 0)
      fCandleBoxRange = 0;
   else if (bRange > 1)
      fCandleBoxRange = 1;
   else
      fCandleBoxRange = bRange;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the line width of the circle marker of a candle plot ([1,5]).

void TStyle::SetCandleCircleLineWidth(Int_t CircleLineWidth)
{
   if (CircleLineWidth<1 || CircleLineWidth>5) {
      Error("SetCandleCircleLineWidth", "illegal line width %d. It must be in the range [1,5]\n", (int)CircleLineWidth);
      fCandleCircleLineWidth = 1;
      return;
   }
   fCandleCircleLineWidth = CircleLineWidth;
}

////////////////////////////////////////////////////////////////////////////////
/// Set the line width of the cross marker of a candle plot ([1,5]).

void TStyle::SetCandleCrossLineWidth(Int_t CrossLineWidth)
{
   if (CrossLineWidth<1 || CrossLineWidth>5) {
      Error("SetCandleCrossLineWidth", "illegal line width %d. It must be in the range [1,5]\n", (int)CrossLineWidth);
      fCandleCrossLineWidth = 1;
      return;
   }
   fCandleCrossLineWidth = CrossLineWidth;
}


////////////////////////////////////////////////////////////////////////////////
/// Save the current style in a C++ macro file.

void TStyle::SaveSource(const char *filename, Option_t *option)
{
   // Opens a file named filename or "Rootstyl.C"
   TString ff = filename && *filename ? filename : "Rootstyl.C";

   // Computes the main method name.
   TString sname, fname = gSystem->BaseName(ff);
   auto pos = fname.First('.');

   if (pos == kNPOS) {
      sname = fname;
      ff.Append(".C");
   } else
      sname = fname(0, pos);

   // Tries to open the file.
   std::ofstream out;
   out.open(ff.Data(), std::ios::out);
   if (!out.good()) {
      Error("SaveSource", "cannot open file: %s", ff.Data());
      return;
   }

   // Writes macro header, date/time stamp as string, and the used Root version
   out << "// Macro generated from application: " << gApplication->Argv(0) << "\n";
   out << "// By ROOT version " << gROOT->GetVersion() << " on " << TDatime().AsSQLString() << "\n\n";

   // Writes include.
   out << "#include \"TStyle.h\"\n\n";

   // Writes the macro entry point equal to the fname
   out << "void " << sname << "()\n";
   out << "{\n";

   TStyle::SavePrimitive(out, option);

   out << "}\n";
   out.close();

   printf(" C++ macro file %s has been generated\n", gSystem->BaseName(ff));
}

////////////////////////////////////////////////////////////////////////////////
/// Save primitive as a C++ statement(s) on output stream out

void TStyle::SavePrimitive(std::ostream &out, Option_t * /*= ""*/)
{
   auto asBool = [](bool flag) { return flag ? "kTRUE" : "kFALSE"; };

   out << "   // Add the saved style to the current ROOT session.\n";
   out << "   \n";
   out << "   delete gROOT->GetStyle(\"" << GetName() << "\");\n";

   SavePrimitiveConstructor(
      out, Class(), "tmpStyle",
      TString::Format("\"%s\", \"%s\"", GetName(), TString(GetTitle()).ReplaceSpecialCppChars().Data()));

   const char *prefix = "   tmpStyle->";

   // fXAxis, fYAxis and fZAxis
   out << prefix << "SetNdivisions(" << GetNdivisions("x") << ", \"x\");\n";
   out << prefix << "SetNdivisions(" << GetNdivisions("y") << ", \"y\");\n";
   out << prefix << "SetNdivisions(" << GetNdivisions("z") << ", \"z\");\n";
   out << prefix << "SetAxisColor(" << TColor::SavePrimitiveColor(GetAxisColor("x")) << ", \"x\");\n";
   out << prefix << "SetAxisColor(" << TColor::SavePrimitiveColor(GetAxisColor("y")) << ", \"y\");\n";
   out << prefix << "SetAxisColor(" << TColor::SavePrimitiveColor(GetAxisColor("z")) << ", \"z\");\n";
   out << prefix << "SetLabelColor(" << TColor::SavePrimitiveColor(GetLabelColor("x")) << ", \"x\");\n";
   out << prefix << "SetLabelColor(" << TColor::SavePrimitiveColor(GetLabelColor("y")) << ", \"y\");\n";
   out << prefix << "SetLabelColor(" << TColor::SavePrimitiveColor(GetLabelColor("z")) << ", \"z\");\n";
   out << prefix << "SetLabelFont(" << GetLabelFont("x") << ", \"x\");\n";
   out << prefix << "SetLabelFont(" << GetLabelFont("y") << ", \"y\");\n";
   out << prefix << "SetLabelFont(" << GetLabelFont("z") << ", \"z\");\n";
   out << prefix << "SetLabelOffset(" << GetLabelOffset("x") << ", \"x\");\n";
   out << prefix << "SetLabelOffset(" << GetLabelOffset("y") << ", \"y\");\n";
   out << prefix << "SetLabelOffset(" << GetLabelOffset("z") << ", \"z\");\n";
   out << prefix << "SetLabelSize(" << GetLabelSize("x") << ", \"x\");\n";
   out << prefix << "SetLabelSize(" << GetLabelSize("y") << ", \"y\");\n";
   out << prefix << "SetLabelSize(" << GetLabelSize("z") << ", \"z\");\n";
   out << prefix << "SetTickLength(" << GetTickLength("x") << ", \"x\");\n";
   out << prefix << "SetTickLength(" << GetTickLength("y") << ", \"y\");\n";
   out << prefix << "SetTickLength(" << GetTickLength("z") << ", \"z\");\n";
   out << prefix << "SetTitleOffset(" << GetTitleOffset("x") << ", \"x\");\n";
   out << prefix << "SetTitleOffset(" << GetTitleOffset("y") << ", \"y\");\n";
   out << prefix << "SetTitleOffset(" << GetTitleOffset("z") << ", \"z\");\n";
   out << prefix << "SetTitleSize(" << GetTitleSize("x") << ", \"x\");\n";
   out << prefix << "SetTitleSize(" << GetTitleSize("y") << ", \"y\");\n";
   out << prefix << "SetTitleSize(" << GetTitleSize("z") << ", \"z\");\n";
   out << prefix << "SetTitleColor(" << TColor::SavePrimitiveColor(GetTitleColor("x")) << ", \"x\");\n";
   out << prefix << "SetTitleColor(" << TColor::SavePrimitiveColor(GetTitleColor("y")) << ", \"y\");\n";
   out << prefix << "SetTitleColor(" << TColor::SavePrimitiveColor(GetTitleColor("z")) << ", \"z\");\n";
   out << prefix << "SetTitleFont(" << GetTitleFont("x") << ", \"x\");\n";
   out << prefix << "SetTitleFont(" << GetTitleFont("y") << ", \"y\");\n";
   out << prefix << "SetTitleFont(" << GetTitleFont("z") << ", \"z\");\n";

   out << prefix << "SetExponentOffset(" << fXAxisExpXOffset << ", " << fXAxisExpYOffset << ", \"x\");\n";
   out << prefix << "SetExponentOffset(" << fYAxisExpXOffset << ", " << fYAxisExpYOffset << ", \"y\");\n";
   out << prefix << "SetAxisMaxDigits(" << GetAxisMaxDigits() << ");\n";

   out << prefix << "SetBarWidth(" << GetBarWidth() << ");\n";
   out << prefix << "SetBarOffset(" << GetBarOffset() << ");\n";
   out << prefix << "SetDrawBorder(" << GetDrawBorder() << ");\n";
   out << prefix << "SetOptLogx(" << GetOptLogx() << ");\n";
   out << prefix << "SetOptLogy(" << GetOptLogy() << ");\n";
   out << prefix << "SetOptLogz(" << GetOptLogz() << ");\n";
   out << prefix << "SetOptDate(" << GetOptDate() << ");\n";
   out << prefix << "SetOptStat(" << GetOptStat() << ");\n";
   out << prefix << "SetOptTitle(" << GetOptTitle() << ");\n";
   out << prefix << "SetOptFit(" << GetOptFit() << ");\n";
   out << prefix << "SetNumberContours(" << GetNumberContours() << ");\n";

   // fAttDate
   out << prefix << "GetAttDate()->SetTextFont(" << GetAttDate()->GetTextFont() << ");\n";
   out << prefix << "GetAttDate()->SetTextSize(" << GetAttDate()->GetTextSize() << ");\n";
   out << prefix << "GetAttDate()->SetTextAngle(" << GetAttDate()->GetTextAngle() << ");\n";
   out << prefix << "GetAttDate()->SetTextAlign(" << GetAttDate()->GetTextAlign() << ");\n";
   out << prefix << "GetAttDate()->SetTextColor(" << TColor::SavePrimitiveColor(GetAttDate()->GetTextColor()) << ");\n";

   out << prefix << "SetDateX(" << GetDateX() << ");\n";
   out << prefix << "SetDateY(" << GetDateY() << ");\n";
   out << prefix << "SetEndErrorSize(" << GetEndErrorSize() << ");\n";
   out << prefix << "SetErrorX(" << GetErrorX() << ");\n";
   out << prefix << "SetFuncColor(" << TColor::SavePrimitiveColor(GetFuncColor()) << ");\n";
   out << prefix << "SetFuncStyle(" << GetFuncStyle() << ");\n";
   out << prefix << "SetFuncWidth(" << GetFuncWidth() << ");\n";
   out << prefix << "SetGridColor(" << TColor::SavePrimitiveColor(GetGridColor()) << ");\n";
   out << prefix << "SetGridStyle(" << GetGridStyle() << ");\n";
   out << prefix << "SetGridWidth(" << GetGridWidth() << ");\n";
   out << prefix << "SetLegendBorderSize(" << GetLegendBorderSize() << ");\n";
   out << prefix << "SetLegendFillColor(" << TColor::SavePrimitiveColor(GetLegendFillColor()) << ");\n";
   out << prefix << "SetLegendFillStyle(" << GetLegendFillStyle() << ");\n";
   out << prefix << "SetLegendFont(" << GetLegendFont() << ");\n";
   out << prefix << "SetLegendTextSize(" << GetLegendTextSize() << ");\n";
   out << prefix << "SetHatchesLineWidth(" << GetHatchesLineWidth() << ");\n";
   out << prefix << "SetHatchesSpacing(" << GetHatchesSpacing() << ");\n";
   out << prefix << "SetFrameFillColor(" << TColor::SavePrimitiveColor(GetFrameFillColor()) << ");\n";
   out << prefix << "SetFrameLineColor(" << TColor::SavePrimitiveColor(GetFrameLineColor()) << ");\n";
   out << prefix << "SetFrameFillStyle(" << GetFrameFillStyle() << ");\n";
   out << prefix << "SetFrameLineStyle(" << GetFrameLineStyle() << ");\n";
   out << prefix << "SetFrameLineWidth(" << GetFrameLineWidth() << ");\n";
   out << prefix << "SetFrameBorderSize(" << GetFrameBorderSize() << ");\n";
   out << prefix << "SetFrameBorderMode(" << GetFrameBorderMode() << ");\n";
   out << prefix << "SetHistFillColor(" << TColor::SavePrimitiveColor(GetHistFillColor()) << ");\n";
   out << prefix << "SetHistLineColor(" << TColor::SavePrimitiveColor(GetHistLineColor()) << ");\n";
   out << prefix << "SetHistFillStyle(" << GetHistFillStyle() << ");\n";
   out << prefix << "SetHistLineStyle(" << GetHistLineStyle() << ");\n";
   out << prefix << "SetHistLineWidth(" << GetHistLineWidth() << ");\n";
   out << prefix << "SetHistMinimumZero(" << asBool(GetHistMinimumZero()) << ");\n";
   out << prefix << "SetCanvasPreferGL(" << asBool(GetCanvasPreferGL()) << ");\n";
   out << prefix << "SetCanvasColor(" << TColor::SavePrimitiveColor(GetCanvasColor()) << ");\n";
   out << prefix << "SetCanvasBorderSize(" << GetCanvasBorderSize() << ");\n";
   out << prefix << "SetCanvasBorderMode(" << GetCanvasBorderMode() << ");\n";
   out << prefix << "SetCanvasDefH(" << GetCanvasDefH() << ");\n";
   out << prefix << "SetCanvasDefW(" << GetCanvasDefW() << ");\n";
   out << prefix << "SetCanvasDefX(" << GetCanvasDefX() << ");\n";
   out << prefix << "SetCanvasDefY(" << GetCanvasDefY() << ");\n";
   out << prefix << "SetPadColor(" << TColor::SavePrimitiveColor(GetPadColor()) << ");\n";
   out << prefix << "SetPadBorderSize(" << GetPadBorderSize() << ");\n";
   out << prefix << "SetPadBorderMode(" << GetPadBorderMode() << ");\n";
   out << prefix << "SetPadBottomMargin(" << GetPadBottomMargin() << ");\n";
   out << prefix << "SetPadTopMargin(" << GetPadTopMargin() << ");\n";
   out << prefix << "SetPadLeftMargin(" << GetPadLeftMargin() << ");\n";
   out << prefix << "SetPadRightMargin(" << GetPadRightMargin() << ");\n";
   out << prefix << "SetPadGridX(" << asBool(GetPadGridX()) << ");\n";
   out << prefix << "SetPadGridY(" << asBool(GetPadGridY()) << ");\n";
   out << prefix << "SetPadTickX(" << GetPadTickX() << ");\n";
   out << prefix << "SetPadTickY(" << GetPadTickY() << ");\n";
   out << prefix << "SetOrthoCamera(" << asBool(GetOrthoCamera()) << ");\n";

   out << prefix << "SetCandleWhiskerRange(" << GetCandleWhiskerRange() << ");\n";
   out << prefix << "SetCandleBoxRange(" << GetCandleBoxRange() << ");\n";
   out << prefix << "SetCandleScaled(" << asBool(GetCandleScaled()) << ");\n";
   out << prefix << "SetViolinScaled(" << asBool(GetViolinScaled()) << ");\n";

   // fPaperSizeX, fPaperSizeY
   out << prefix << "SetPaperSize(" << fPaperSizeX << ", " << fPaperSizeY << ");\n";

   out << prefix << "SetScreenFactor(" << GetScreenFactor() << ");\n";
   out << prefix << "SetStatColor(" << TColor::SavePrimitiveColor(GetStatColor()) << ");\n";
   out << prefix << "SetStatTextColor(" << TColor::SavePrimitiveColor(GetStatTextColor()) << ");\n";
   out << prefix << "SetStatBorderSize(" << GetStatBorderSize() << ");\n";
   out << prefix << "SetStatFont(" << GetStatFont() << ");\n";
   out << prefix << "SetStatFontSize(" << GetStatFontSize() << ");\n";
   out << prefix << "SetStatStyle(" << GetStatStyle() << ");\n";
   out << prefix << "SetStatFormat(\"" << GetStatFormat() << "\");\n";
   out << prefix << "SetStatX(" << GetStatX() << ");\n";
   out << prefix << "SetStatY(" << GetStatY() << ");\n";
   out << prefix << "SetStatW(" << GetStatW() << ");\n";
   out << prefix << "SetStatH(" << GetStatH() << ");\n";
   out << prefix << "SetStripDecimals(" << asBool(GetStripDecimals()) << ");\n";
   out << prefix << "SetTitleAlign(" << GetTitleAlign() << ");\n";
   out << prefix << "SetTitleFillColor(" << TColor::SavePrimitiveColor(GetTitleFillColor()) << ");\n";
   out << prefix << "SetTitleTextColor(" << TColor::SavePrimitiveColor(GetTitleTextColor()) << ");\n";
   out << prefix << "SetTitleBorderSize(" << GetTitleBorderSize() << ");\n";
   out << prefix << "SetTitleFont(" << GetTitleFont() << ");\n";
   out << prefix << "SetTitleFontSize(" << GetTitleFontSize() << ");\n";
   out << prefix << "SetTitleStyle(" << GetTitleStyle() << ");\n";
   out << prefix << "SetTitleX(" << GetTitleX() << ");\n";
   out << prefix << "SetTitleY(" << GetTitleY() << ");\n";
   out << prefix << "SetTitleW(" << GetTitleW() << ");\n";
   out << prefix << "SetTitleH(" << GetTitleH() << ");\n";
   out << prefix << "SetLegoInnerR(" << GetLegoInnerR() << ");\n";

   out << "  \n";

   // Palette belongs to the TColor
   // but stored together with any TStyle instance
   TColor::SaveColorsPalette(out);

   out << "  \n";

   // fLineStyle
   for (Int_t li = 0; li < 30; ++li)
      out << prefix << "SetLineStyleString(" << li << ", \"" << TString(fLineStyle[li]).ReplaceSpecialCppChars()
          << "\");\n";

   out << "  \n";

   out << prefix << "SetHeaderPS(\"" << TString(GetHeaderPS()).ReplaceSpecialCppChars() << "\");\n";
   out << prefix << "SetTitlePS(\"" << TString(GetTitlePS()).ReplaceSpecialCppChars() << "\");\n";
   out << prefix << "SetFitFormat(\"" << TString(GetFitFormat()).ReplaceSpecialCppChars() << "\");\n";
   out << prefix << "SetPaintTextFormat(\"" << TString(GetPaintTextFormat()).ReplaceSpecialCppChars() << "\");\n";
   out << prefix << "SetLineScalePS(" << GetLineScalePS() << ");\n";
   out << prefix << "SetJoinLinePS(" << GetJoinLinePS() << ");\n";
   out << prefix << "SetCapLinePS(" << GetCapLinePS() << ");\n";
   out << prefix << "SetColorModelPS(" << GetColorModelPS() << ");\n";
   out << prefix << "SetTimeOffset(" << TString::Format("%9.0f", GetTimeOffset()) << ");\n";

   // Inheritance :
   SaveLineAttributes(out, "tmpStyle", -1, -1, -1);
   SaveFillAttributes(out, "tmpStyle", -1, -1);
   SaveMarkerAttributes(out, "tmpStyle", -1, -1, -1);
   SaveTextAttributes(out, "tmpStyle", 0, 0, 0, 0, 0);
}
