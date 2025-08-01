// @(#)root/roostats:$Id$
// Author: Kyle Cranmer, Lorenzo Moneta, Gregory Schott, Wouter Verkerke
// Additional Contributions: Giovanni Petrucciani
/*************************************************************************
 * Copyright (C) 1995-2008, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

/** \class RooStats::ProfileLikelihoodTestStat
    \ingroup Roostats

ProfileLikelihoodTestStat is an implementation of the TestStatistic interface
that calculates the profile likelihood ratio at a particular parameter point
given a dataset. It does not constitute a statistical test, for that one may
either use:

  - the ProfileLikelihoodCalculator that relies on asymptotic properties of the
    Profile Likelihood Ratio
  - the NeymanConstruction class with this class as a test statistic
  - the HybridCalculator class with this class as a test statistic


*/

#include "RooStats/ProfileLikelihoodTestStat.h"
#include "RooFitResult.h"
#include "RooPullVar.h"
#include "RooStats/DetailedOutputAggregator.h"

#include "RooProfileLL.h"
#include "RooMsgService.h"
#include "RooMinimizer.h"
#include "RooArgSet.h"
#include "RooDataSet.h"
#include "TStopwatch.h"

#include "RooStats/RooStatsUtils.h"

bool RooStats::ProfileLikelihoodTestStat::fgAlwaysReuseNll = true ;

void RooStats::ProfileLikelihoodTestStat::SetAlwaysReuseNLL(bool flag) { fgAlwaysReuseNll = flag ; }

/// Check if there are non-const parameters so it is worth to do the minimization.
bool RooStats::ProfileLikelihoodTestStat::minimizationNeeded(RooArgSet allParams) const
{
   allParams.removeConstantParameters();

   return !allParams.empty();
}

std::pair<double, int> RooStats::ProfileLikelihoodTestStat::minimizeNLL(std::string const &prefix)
{
   // minimize and count eval errors
   fNll->clearEvalErrorLog();
   std::unique_ptr<RooFitResult> result{GetMinNLL()};
   if (!result) {
      // this should not really happen
      throw std::runtime_error("ProfileLikelihoodTestStat: minimization unexpectedly failed!");
   }
   double minNll = result->minNll();
   double status = result->status();

   // save this snapshot
   if (fDetailedOutputEnabled) {
      std::unique_ptr<RooArgSet> detOutput{
         DetailedOutputAggregator::GetAsArgSet(result.get(), prefix, fDetailedOutputWithErrorsAndPulls)};
      fDetailedOutput->addOwned(*detOutput);
   }

   return {minNll, status};
}

////////////////////////////////////////////////////////////////////////////////
/// internal function to evaluate test statistics
/// can do depending on type:
/// -  type  = 0 standard evaluation,
/// -  type = 1 find only unconditional NLL minimum,
/// -  type = 2 conditional MLL

double RooStats::ProfileLikelihoodTestStat::EvaluateProfileLikelihood(int type, RooAbsData& data, RooArgSet& paramsOfInterest) {

       if (fDetailedOutputEnabled) {
          fDetailedOutput = std::make_unique<RooArgSet>();
       }

       //data.Print("V");

       TStopwatch tsw;
       tsw.Start();

       double initial_mu_value  = 0;
       RooRealVar* firstPOI = dynamic_cast<RooRealVar*>(paramsOfInterest.first());
       if (firstPOI) initial_mu_value = firstPOI->getVal();
       //paramsOfInterest.getRealValue(firstPOI->GetName());
       if (fPrintLevel > 1) {
            std::cout << "POIs: " << std::endl;
            paramsOfInterest.Print("v");
       }

       RooFit::MsgLevel msglevel = RooMsgService::instance().globalKillBelow();
       if (fPrintLevel < 3) RooMsgService::instance().setGlobalKillBelow(RooFit::FATAL);

       // simple
       bool reuse=(fReuseNll || fgAlwaysReuseNll) ;

       bool created(false) ;
       if (!reuse || fNll==nullptr) {
          std::unique_ptr<RooArgSet> allParams{fPdf->getParameters(data)};
          RooStats::RemoveConstantParameters(&*allParams);

          // need to call constrain for RooSimultaneous until stripDisconnected problem fixed
          fNll = std::unique_ptr<RooAbsReal>{fPdf->createNLL(data, RooFit::CloneData(false),RooFit::Constrain(*allParams),
                                 RooFit::GlobalObservables(fGlobalObs), RooFit::ConditionalObservables(fConditionalObs), RooFit::Offset(fLOffset))};

          if (fPrintLevel > 0) {
             std::cout << "ProfileLikelihoodTestStat::Evaluate - Use Offset mode \""
                 << fLOffset << "\" in creating NLL" << std::endl;
          }

          created = true ;
          if (fPrintLevel > 1) std::cout << "creating NLL " << &*fNll << " with data = " << &data << std::endl ;
       }
       if (reuse && !created) {
         if (fPrintLevel > 1) std::cout << "reusing NLL " << &*fNll << " new data = " << &data << std::endl ;
         fNll->setData(data,false) ;
       }
       // print data in case of number counting (simple data sets)
       if (fPrintLevel > 1 && data.numEntries() == 1) {
          std::cout << "Data set used is:  ";
          RooStats::PrintListContent(*data.get(0), std::cout);
       }


       // make sure we set the variables attached to this nll
       std::unique_ptr<RooArgSet> attachedSet{fNll->getVariables()};

       attachedSet->assign(paramsOfInterest);
       RooArgSet* origAttachedSet = (RooArgSet*) attachedSet->snapshot();

       ///////////////////////////////////////////////////////////////////////
       // New profiling based on RooMinimizer (allows for Minuit2)
       // based on major speed increases seen by CMS for complex problems


       // other order
       // get the numerator
       RooArgSet* snap =  (RooArgSet*)paramsOfInterest.snapshot();

       tsw.Stop();
       double createTime = tsw.CpuTime();
       tsw.Start();

       // get the denominator
       double uncondML = 0;
       double fit_favored_mu = 0;
       int statusD = 0;
       if (type != 2) {
          if (!minimizationNeeded(*attachedSet)) {
             uncondML = fNll->getVal();
          } else {
             if (fPrintLevel>1) std::cout << "Do unconditional fit" << std::endl;
             std::tie(uncondML, statusD) = minimizeNLL("fitUncond_");
          }

          // get best fit value for one-sided interval
          if (firstPOI) fit_favored_mu = attachedSet->getRealValue(firstPOI->GetName()) ;
       }
       tsw.Stop();
       double fitTime1  = tsw.CpuTime();

       //double ret = 0;
       int statusN = 0;
       tsw.Start();

       double condML = 0;

       bool doConditionalFit = (type != 1);

       // skip the conditional ML (the numerator) only when fit value is smaller than test value
       if (!fSigned && type==0 &&
           ((fLimitType==oneSided          && fit_favored_mu >= initial_mu_value) ||
            (fLimitType==oneSidedDiscovery && fit_favored_mu <= initial_mu_value))) {
          doConditionalFit = false;
          condML = uncondML;
       }

       if (doConditionalFit) {

          if (fPrintLevel>1) std::cout << "Do conditional fit " << std::endl;


          //       std::cout <<" reestablish snapshot"<< std::endl;
          attachedSet->assign(*snap);


          // set the POI to constant
          for (auto *tmpPar : paramsOfInterest) {
             RooRealVar *tmpParA = dynamic_cast<RooRealVar *>(attachedSet->find(tmpPar->GetName()));
             if (tmpParA) tmpParA->setConstant();
          }


          // in case no nuisance parameters are present
          // no need to minimize just evaluate the nll
          if (!minimizationNeeded(*attachedSet)) {
             // be sure to evaluate with offsets
             if (fLOffset == "initial") RooAbsReal::setHideOffset(false);
             condML = fNll->getVal();
             if (fLOffset == "initial") RooAbsReal::setHideOffset(true);
          }
          else {
            std::tie(condML, statusN) = minimizeNLL("fitCond_");
          }

       }

       tsw.Stop();
       double fitTime2 = tsw.CpuTime();

       double pll = 0;
       if (type != 0)  {
          // for conditional only or unconditional fits
          // need to compute nll value without the offset
          if (fLOffset == "initial") {
             RooAbsReal::setHideOffset(false) ;
             pll = fNll->getVal();
          }
          else {
             if (type == 1) {
               pll = uncondML;
             } else if (type == 2) {
               pll = condML;
             }
          }
       }
       else {  // type == 0
          // for standard profile likelihood evaluations
         pll = condML-uncondML;

         if (fSigned) {
            if (pll<0.0) {
               if (fPrintLevel > 0) std::cout << "pll is negative - setting it to zero " << std::endl;
               pll = 0.0;   // bad fit
            }
           if (fLimitType==oneSidedDiscovery ? (fit_favored_mu < initial_mu_value)
                                             : (fit_favored_mu > initial_mu_value))
             pll = -pll;
         }
       }

       if (fPrintLevel > 0) {
          std::cout << "EvaluateProfileLikelihood - ";
          if (type <= 1)
             std::cout << "mu hat = " << fit_favored_mu  <<  ", uncond ML = " << uncondML;
          if (type != 1)
             std::cout << ", cond ML = " << condML;
          if (type == 0)
             std::cout << " pll = " << pll;
          std::cout << " time (create/fit1/2) " << createTime << " , " << fitTime1 << " , " << fitTime2
                    << std::endl;
       }


       // need to restore the values ?
       attachedSet->assign(*origAttachedSet);

       delete origAttachedSet;
       delete snap;

       if (!reuse) {
          fNll.reset();
       }

       RooMsgService::instance().setGlobalKillBelow(msglevel);

       if(statusN!=0 || statusD!=0) {
         return -1; // indicate failed fit (WVE is not used anywhere yet)
       }

       return pll;

     }

////////////////////////////////////////////////////////////////////////////////
/// find minimum of NLL using RooMinimizer

std::unique_ptr<RooFitResult> RooStats::ProfileLikelihoodTestStat::GetMinNLL() {

   const auto& config = GetGlobalRooStatsConfig();
   RooMinimizer minim(*fNll);
   minim.setStrategy(fStrategy);
   minim.setEvalErrorWall(config.useEvalErrorWall);
   //LM: RooMinimizer.setPrintLevel has +1 offset - so subtract  here -1 + an extra -1
   int level = (fPrintLevel == 0) ? -1 : fPrintLevel -2;
   minim.setPrintLevel(level);
   minim.setEps(fTolerance);
   // this causes a memory leak
   minim.optimizeConst(2);
   TString minimizer = fMinimizer;
   TString algorithm = ROOT::Math::MinimizerOptions::DefaultMinimizerAlgo();
   if (algorithm == "Migrad") algorithm = "Minimize"; // prefer to use Minimize instead of Migrad
   int status;
   for (int tries = 1, maxtries = 4; tries <= maxtries; ++tries) {
      status = minim.minimize(minimizer,algorithm);
      if (status%1000 == 0) {  // ignore errors from Improve
         break;
      } else if (tries < maxtries) {
         std::cout << "    ----> Doing a re-scan first" << std::endl;
         minim.minimize(minimizer,"Scan");
         if (tries == 2) {
            if (fStrategy == 0 ) {
               std::cout << "    ----> trying with strategy = 1" << std::endl;
               minim.setStrategy(1);
            }
            else
               tries++; // skip this trial if strategy is already 1
         }
         if (tries == 3) {
            std::cout << "    ----> trying with improve" << std::endl;
            minimizer = "Minuit";
            algorithm = "migradimproved";
         }
      }
   }

   //how to get cov quality faster?
   return std::unique_ptr<RooFitResult>{minim.save()};
   //minim.optimizeConst(false);
}
