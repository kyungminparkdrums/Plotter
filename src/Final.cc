#include <TTree.h>
#include <TH1.h>
#include <TFile.h>
#include <TDirectory.h>
#include <TClass.h>
#include <TKey.h>
#include <TGraphAsymmErrors.h>
#include <TChain.h>
#include <TCanvas.h>
#include <TText.h>
#include <THStack.h>
#include <TPaveText.h>
#include <TGraphErrors.h>
#include <TLegend.h>
#include <TF1.h>
#include <TStyle.h>
#include <TROOT.h>

#include <vector>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <map>
#include "tokenizer.hpp"
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <array>

using namespace std;

struct Normer {
  vector<string> input;
  vector<double> skim, xsec;
  string output, type="";

  TList* FileList;
  vector<double> events, CumulativeEfficiency, scaleFactor, scaleFactorError;
  bool use, isData=false;

  void print() {
    cout << " =========== " << output << " =========== " << endl;
    for(int i = 0; i < input.size(); ++i) {
      cout << input.at(i) ;
      if(isData) cout << endl;
      else cout << " " << skim.at(i) << " " << xsec.at(i) <<  endl;
    }
    cout << endl;
  }
};

struct Plot {
  TList* FileList[3] = {new TList(), new TList(), new TList()};
  string style;

  int color[10] = {100, 90, 80, 70, 60, 95, 85, 75, 65, 55};

  void addFile(string type, TObject* obj) {
    if(type == "data") FileList[0]->Add(obj);
    else if(type == "bg") FileList[1]->Add(obj);
    else if(type == "sig") FileList[2]->Add(obj);
  }
};

struct Style {
  TStyle* styler = new TStyle("Styler", "Style");
  Style() {
    setStyle();
  }
  void setStyle() {
    styler->SetOptStat(0);
    styler->SetOptTitle(0);
    styler->SetPalette(1,0);
    styler->SetOptStat(0);
    styler->SetOptTitle(0);
    styler->SetOptDate(0);
    styler->SetLabelSize(0.05, "xy");
    styler->SetTitleSize(0.12,"xy");
    styler->SetLabelOffset(0.007, "xy");
    styler->SetTitleOffset(0.9, "xy");
    styler->SetTitleFont(42,"xy");
    styler->SetLabelFont(42, "xy");
    styler->SetCanvasColor(0);
    styler->SetCanvasBorderMode(0);
    styler->SetCanvasBorderSize(3);

    styler->SetPadBottomMargin(0.05);
    styler->SetPadTopMargin(0.05);
    styler->SetPadLeftMargin(0.15);
    styler->SetPadRightMargin(0.05);
    styler->SetPadGridX(0);
    styler->SetPadGridY(0);
    styler->SetPadTickX(1);
    styler->SetPadTickY(1);
  
    styler->SetFrameBorderMode(0);
  }
  TStyle* getStyle() {
    return styler;
  }
};



map<string, Normer*> plots;
Plot fullPlot;
double lumi;
string output;

void read_info(string);
int getModTime(const char*);
bool shouldAdd(string, string);
void MergeRootfile( TDirectory*, Normer&);

void CreateStack( TDirectory*, Plot&);
THStack* sortStack(THStack*);
TLegend* createLeg(TList* bgl=NULL, TList* sigl=NULL);
TGraphErrors* createError(TH1*, bool);
void sizePad(double, TVirtualPad*, bool);
TF1* createLine(TH1*);
void setXAxisTop(TH1*, TH1*, THStack*);
void setYAxisTop(TH1*, TH1*, double, THStack*);
void setXAxisBot(TH1*, TAxis*, double);
void setYAxisBot(TH1*, double);



Style stylez;


int main(int argc, char* argv[]) {
  if(argc < 2) {
    cerr << "No config file given: Exiting" << endl;
    exit(1);
  }
  for(int i = 1; i < argc; ++i) {
    read_info(argv[i]);
  }


  for(map<string, Normer*>::iterator it = plots.begin(); it != plots.end(); ++it) {
    if(! it->second->use) continue;
    it->second->print();
    it->second->FileList = new TList();
    for(vector<string>::iterator name = it->second->input.begin(); name != it->second->input.end(); ++name) {
      it->second->FileList->Add(TFile::Open(name->c_str()));
    }
    string filename = it->second->output;
    while(filename.find("#") != string::npos) {
      filename.erase(filename.find("#"), 1);
    }
    TFile* final = new TFile(filename.c_str(), "RECREATE");
    final->SetTitle(it->second->output.c_str());
    MergeRootfile(final, *it->second);
    fullPlot.addFile(it->second->type, final);
  }
  TFile* final = new TFile(output.c_str(), "RECREATE");

  gStyle = stylez.getStyle();
  CreateStack(final, fullPlot);
  final->Close();

}


void read_info(string filename) {
  typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
  ifstream info_file(filename);
  boost::char_separator<char> sep(", \t");

  if(!info_file) {
    std::cout << "could not open file " << filename <<std::endl;
    exit(1);
  }

  vector<string> stemp;
  string group, line;
  while(getline(info_file, line)) {
    tokenizer tokens(line, sep);
    stemp.clear();
    for(tokenizer::iterator iter = tokens.begin();iter != tokens.end(); iter++) {
      if( ((*iter)[0] == '/' && (*iter)[0] == '/') || ((*iter)[0] == '#') ) break;
      stemp.push_back(*iter);

    }
    if(stemp.size() >= 2) {
      if(stemp[0].find("lumi") != string::npos) lumi = stod(stemp[1]);
      else if(stemp[0].find("output") != string::npos) output = stemp[1];
      else if(plots.find(stemp[1]) == plots.end()) { 
	Normer* tmpplot = new Normer;
	tmpplot->input.push_back(stemp[0]);
	tmpplot->output = stemp[1];
	tmpplot->use = shouldAdd(stemp[0], stemp[1]);
	tmpplot->CumulativeEfficiency.push_back(1.);
	tmpplot->scaleFactor.push_back(1.);
	tmpplot->scaleFactorError.push_back(0.);
	tmpplot->events.push_back(0);
	if(stemp.size() == 2) {
	  tmpplot->xsec.push_back(1.0);
	  tmpplot->skim.push_back(1.0);
	  tmpplot->isData = true;
	  tmpplot->type = "data";
	}    
	
	plots[stemp[1]] = tmpplot;
      } else {
	plots[stemp[1]]->input.push_back(stemp[0]);
	plots[stemp[1]]->use *= shouldAdd(stemp[0], stemp[1]);
	plots[stemp[1]]->CumulativeEfficiency.push_back(1.);
	plots[stemp[1]]->scaleFactor.push_back(1.);
	plots[stemp[1]]->scaleFactorError.push_back(0.);
	plots[stemp[1]]->events.push_back(0);
	if(stemp.size() == 2) {
	  plots[stemp[1]]->xsec.push_back(1.0);
	  plots[stemp[1]]->skim.push_back(1.0);
	}    
      }
      if(stemp.size() == 5) {
	plots[stemp[1]]->xsec.push_back(stod(stemp[2]));
	plots[stemp[1]]->skim.push_back(stod(stemp[3]));
	if(plots[stemp[1]]->type == "") plots[stemp[1]]->type = stemp[4];
      }

    } 
  }
  info_file.close();

}

bool shouldAdd(string infile, string globalFile) {
  struct stat buffer;
  if(stat(infile.c_str(), &buffer) != 0) return false;
  ////need to change so doesn't make plot if fatal error (eg file doesn't exist!
  else if(stat(globalFile.c_str(), &buffer) != 0) return true;
  else if(getModTime(globalFile.c_str()) > getModTime(infile.c_str())) return true;
  else return true;

}

int getModTime(const char *path) {
  struct stat attr;
  stat(path, &attr);
  char date[100] = {0};
  strftime(date, 100, "%s", localtime(&attr.st_mtime));
  return atoi(date);

}

void MergeRootfile( TDirectory *target, Normer& plotter ) {

  TList* sourcelist = plotter.FileList;

  TString path( (char*)strstr( target->GetPath(), ":" ) );
  path.Remove( 0, 2 );

  TFile *first_source = (TFile*)sourcelist->First();
  first_source->cd( path );
  TDirectory *current_sourcedir = gDirectory;
  //gain time, do not add the objects in the list in memory
  Bool_t status = TH1::AddDirectoryStatus();
  TH1::AddDirectory(kFALSE);


  ///try to find events to calculate efficiency
  TH1F* events;
  current_sourcedir->GetObject("Events", events);

  if(events) {
    int nplot = 0;

    plotter.events.at(nplot) = events->GetBinContent(2);
    plotter.CumulativeEfficiency.at(nplot) = events->GetBinContent(2)/ events->GetBinContent(1);

    TFile *nextsource = (TFile*)sourcelist->After( first_source );
    while( nextsource) {
      nplot++;
      nextsource->cd(path);
      gDirectory->GetObject("Events", events);
      plotter.events.at(nplot) = events->GetBinContent(2);
      plotter.CumulativeEfficiency.at(nplot) = events->GetBinContent(2)/ events->GetBinContent(1);

      nextsource = (TFile*)sourcelist->After( nextsource );
    }
  }
  delete events;

  // loop over all keys in this directory
  TChain *globChain = 0;
  TIter nextkey( current_sourcedir->GetListOfKeys() );
  TKey *key, *oldkey=0;
  while ( (key = (TKey*)nextkey())) {

    //keep only the highest cycle number for each key
    if (oldkey && !strcmp(oldkey->GetName(),key->GetName())) continue;

    first_source->cd( path );

    TObject *obj = key->ReadObj();
    if ( obj->IsA()->InheritsFrom( TH1::Class() ) ) {
      TH1 *h1 = (TH1*)obj;
      h1->Sumw2();
      int spot = 0;
      if(!plotter.isData) h1->Scale(plotter.CumulativeEfficiency.at(spot)/plotter.events.at(spot) * plotter.xsec.at(spot)* lumi* plotter.skim.at(spot));

      TFile *nextsource = (TFile*)sourcelist->After( first_source );
      
      while ( nextsource ) {
	spot++;
	nextsource->cd( path );
	TKey *key2 = (TKey*)gDirectory->GetListOfKeys()->FindObject(h1->GetName());
	if (key2) {
	  TH1 *h2 = (TH1*)key2->ReadObj();
	  h2->Sumw2();
	  double scale = (plotter.isData) ? 1.0 : plotter.CumulativeEfficiency.at(spot)/plotter.events.at(spot) * plotter.xsec.at(spot)* lumi* plotter.skim.at(spot);
	  h1->Add( h2, scale);
	  delete h2;
	  
	}
	nextsource = (TFile*)sourcelist->After( nextsource );
      }
      ////////////////////////////////////////////////////////////
      ////  To gain back Poisson error, uncomment this line /////
      ////////////////////////////////////////////////////////////

      for(int ibin=0; ibin < (h1->GetXaxis()->GetNbins() + 1); ibin++) {
      	h1->SetBinError(ibin, sqrt(pow(h1->GetBinError(ibin),2.0) + h1->GetBinContent(ibin)) );
      }

    }
    else if ( obj->IsA()->InheritsFrom( TTree::Class() ) ) {

      // loop over all source files create a chain of Trees "globChain"
      const char* obj_name= obj->GetName();

      globChain = new TChain(obj_name);
      globChain->Add(first_source->GetName());
      TFile *nextsource = (TFile*)sourcelist->After( first_source );
      //      const char* file_name = nextsource->GetName();
      // cout << "file name  " << file_name << endl;
      while ( nextsource ) {
	  
	globChain->Add(nextsource->GetName());
	nextsource = (TFile*)sourcelist->After( nextsource );
      }
	
    } else if ( obj->IsA()->InheritsFrom( TDirectory::Class() ) ) {
      // it's a subdirectory

      //      cout << "Found subdirectory " << obj->GetName() << endl;

      // create a new subdir of same name and title in the target file
      target->cd();
      TDirectory *newdir = target->mkdir( obj->GetName(), obj->GetTitle() );

      // newdir is now the starting point of another round of merging
      // newdir still knows its depth within the target file via
      // GetPath(), so we can still figure out where we are in the recursion
      MergeRootfile( newdir, plotter );

    } else {

      // object is of no type that we know or can handle
      cout << "Unknown object type, name: "
	   << obj->GetName() << " title: " << obj->GetTitle() << endl;
    }

    // now write the merged histogram (which is "in" obj) to the target file
    // note that this will just store obj in the current directory level,
    // which is not persistent until the complete directory itself is stored
    // by "target->Write()" below
    if ( obj ) {
      target->cd();
      if(obj->IsA()->InheritsFrom( TTree::Class() ))
	globChain->Merge(target->GetFile(),0,"keep");
      else
	obj->Write( key->GetName() );
    }

  } // while ( ( TKey *key = (TKey*)nextkey() ) )

  // save modifications to target file
  target->SaveSelf(kTRUE);
  TH1::AddDirectory(status);
}

/*-----------------------------*/


void CreateStack( TDirectory *target, Plot& plot) {

  TList* datalist = plot.FileList[0];
  TList* bglist = plot.FileList[1];
  TList* sglist = plot.FileList[2];

  TString path( (char*)strstr( target->GetPath(), ":" ) );
  path.Remove( 0, 2 );

  TFile *dataStart = (TFile*)datalist->First();
  dataStart->cd( path );
  TDirectory *current_sourcedir = gDirectory;

  Bool_t status = TH1::AddDirectoryStatus();
  TH1::AddDirectory(kFALSE);


  // loop over all keys in this directory
  TChain *globChain = 0;
  TIter nextkey( current_sourcedir->GetListOfKeys() );
  TKey *key, *oldkey=0;
  while ( (key = (TKey*)nextkey())) {

    //keep only the highest cycle number for each key
    if (oldkey && !strcmp(oldkey->GetName(),key->GetName())) continue;

    dataStart->cd( path );

    TObject *obj = key->ReadObj();
    if ( obj->IsA() ==  TH1F::Class() ) {
      
      TH1 *h1 = (TH1*)obj;
      TH1D* error = new TH1D("error", "error", h1->GetXaxis()->GetNbins(), h1->GetXaxis()->GetXmin(), h1->GetXaxis()->GetXmax());
      TH1D* datahist = new TH1D("data", "data", h1->GetXaxis()->GetNbins(), h1->GetXaxis()->GetXmin(), h1->GetXaxis()->GetXmax());
      TH1D* sighist = new TH1D("signal", "signal", h1->GetXaxis()->GetNbins(), h1->GetXaxis()->GetXmin(), h1->GetXaxis()->GetXmax());
      THStack *hs = new THStack(h1->GetName(),h1->GetName());
      
      /*------------data--------------*/
      datahist->Add(h1);
      TFile *nextfile = (TFile*)datalist->After(dataStart);

      while ( nextfile ) {
	nextfile->cd( path );
	TKey *key2 = (TKey*)gDirectory->GetListOfKeys()->FindObject(h1->GetName());
	if(key2) {
	  TH1* h2 = (TH1*)key2->ReadObj();
	  datahist->Add(h2);
	  delete h2;
	}
	nextfile = (TFile*)datalist->After(nextfile);
      }

      /*------------background--------------*/
      int nfile = 0;
      nextfile = (TFile*)bglist->First();

      while ( nextfile ) {

	nextfile->cd( path );
	TKey *key2 = (TKey*)gDirectory->GetListOfKeys()->FindObject(h1->GetName());
	if(key2) {
	  TH1* h2 = (TH1*)key2->ReadObj();
	  error->Add(h2);
	  for(int i = 1; i < h2->GetXaxis()->GetNbins()+1; i++) {
	    h2->SetBinError(i, 0);
	  }
	  //////style
	  string title = nextfile->GetTitle();
	  title = title.substr(0, title.size()-5);
	  h2->SetTitle(title.c_str());
	  h2->SetLineColor(1);
	  h2->SetFillStyle(1001);
	  h2->SetFillColor(plot.color[nfile]);
	  //////endstyle

	  hs->Add(h2);

	}
	nextfile = (TFile*)bglist->After(nextfile);
	nfile++;
      }


      /*------------signal--------------*/

      /*--------------write out------------*/

      ///data
      datahist->SetMarkerStyle(20);
      datahist->SetLineColor(1);

      ///stack
      hs = sortStack(hs);

      ///legend
      TLegend* legend = createLeg(hs->GetHists());
      legend->AddEntry(datahist, "Data", "f");
      
      //error
      TGraphErrors* errorstack = createError(error, false);
      TGraphErrors* errorratio = createError(error, true);
      
      // data/mc
      TH1D* data_mc = (TH1D*)datahist->Clone("data_mc");
      data_mc->Divide(error);

      //line
      TF1 *PrevFitTMP = createLine(data_mc);

      double padratio = 3, heightratio = 15;




      target->cd();

      if(hs->GetNhists() == 0) continue;

      TCanvas *c = new TCanvas(h1->GetName(), h1->GetName());//403,50,600,600);
      c->Divide(1,2);
      c->cd(1);
      sizePad(padratio, gPad, true);

      hs->Draw();
      datahist->Draw("sameep");
      errorstack->Draw("2");
      legend->Draw();
      setXAxisTop(datahist, error, hs);
      setYAxisTop(datahist, error, heightratio, hs);

      c->cd(2);
      sizePad(padratio, gPad, false);

      data_mc->Draw("ep1");
      errorratio->Draw("2");
      setXAxisBot(data_mc, hs->GetXaxis(), padratio);
      setYAxisBot(data_mc, padratio);



      c->cd();
      c->Write(c->GetName());
      c->Close();

      delete error;
      delete datahist;
      delete sighist;
      delete hs;
      delete legend;
      delete errorstack;
      delete errorratio;
      delete PrevFitTMP;

    } else if ( obj->IsA()->InheritsFrom( TDirectory::Class() ) ) {
      target->cd();
      TDirectory *newdir = target->mkdir( obj->GetName(), obj->GetTitle() );

      CreateStack( newdir, plot );

    } else if ( obj->IsA()->InheritsFrom( TH1::Class() ) ) {
      continue;

    } else {
         cout << "Unknown object type, name: "
	   << obj->GetName() << " title: " << obj->GetTitle() << endl;
    }
  }
  // save modifications to target file
  //  target->SaveSelf(kTRUE);
  TH1::AddDirectory(status);
}

THStack* sortStack(THStack* old) {
  if(old == NULL || old->GetNhists() == 0) return old;
  string name = old->GetName();

  THStack* newstack = new THStack(name.c_str(),name.c_str());

  TList* list = (TList*)old->GetHists();

  while(list->GetSize() > 0) {
    TIter next(list);
    TH1* smallest = NULL;
    TH1* tmp = NULL;
    while ( (tmp = (TH1*)next()) ) {
      if(smallest == NULL || smallest->Integral() > tmp->Integral()) smallest = tmp;
    }
    newstack->Add(smallest);
    list->Remove(smallest);

  }
  
  delete old;
  return newstack;
} 

TLegend* createLeg(TList* bgl, TList* sigl) {
  TLegend* leg = new TLegend(0.6082412,0.4248252,0.8208543,0.7867133);
  vector<TList*> vlist;
  if(bgl!=NULL) vlist.push_back(bgl);
  if(sigl!=NULL) vlist.push_back(sigl);
  for(vector<TList*>::iterator it = vlist.begin(); it != vlist.end(); ++it) {
    TIter next(*it);
    TH1* tmp = NULL;
    while( (tmp = (TH1*)next()) ) {
      leg->AddEntry(tmp, tmp->GetTitle(), "f");
    }
  }
  return leg;

}

TGraphErrors* createError(TH1* error, bool ratio) {
  Double_t mcX[5000];
  Double_t mcY[5000];
  Double_t mcErrorX[5000];
  Double_t mcErrorY[5000];

  for(int bin=0; bin < error->GetXaxis()->GetNbins(); bin++) {
    mcY[bin] = (ratio) ? 1.0 : error->GetBinContent(bin+1);
    mcErrorY[bin] = (ratio) ?  error->GetBinError(bin+1)/error->GetBinContent(bin+1) : error->GetBinError(bin+1);
    mcX[bin] = error->GetBinCenter(bin+1);
    mcErrorX[bin] = error->GetBinWidth(bin+1) * 0.5;
  }
  TGraphErrors *mcError = new TGraphErrors(error->GetXaxis()->GetNbins(),mcX,mcY,mcErrorX,mcErrorY);

  mcError->SetLineWidth(1);
  mcError->SetFillColor(1);
  mcError->SetFillStyle(3002);
  return mcError;
}

void sizePad(double ratio, TVirtualPad* pad, bool isTop) {
  if(isTop)   pad->SetPad("top", "top", 0, 1 / (1.0 + ratio), 1, 1, 0);
  else   pad->SetPad("bottom", "bottom", 0, 0.05, 1, 1 / (1.0 + ratio), 0);
}

TF1* createLine(TH1* data_mc) {
  TF1 *PrevFitTMP = new TF1("PrevFitTMP","pol0",-10000,10000);
  PrevFitTMP->SetFillColor(19);
  PrevFitTMP->SetFillStyle(0);
  PrevFitTMP->SetMarkerStyle(20);
  PrevFitTMP->SetLineColor(2);
  PrevFitTMP->SetLineWidth(1);
  PrevFitTMP->SetParameter(0,1.0);
  PrevFitTMP->SetParError(0,0);
  PrevFitTMP->SetParLimits(0,0,0);
  data_mc->GetListOfFunctions()->Add(PrevFitTMP);
  return PrevFitTMP;
}
 
// pair<TAxis*, TAxis*> createAxistop(TH1* datahist, TH1* error) {
//   int lastbin = 0, firstbin = 1;
//   for(int i = 0; i < datahist->GetXaxis()->GetNbins(); i++) {
//     if(datahist->GetBinContent(i+1) != 0 || error->GetBinContent(i+1) != 0) lastbin = i+1;
//     if(datahist->GetXaxis()->GetXmin() < 0 && firstbin == 1 && (datahist->GetBinContent(i+1) != 0 || error->GetBinContent(i+1) != 0)) firstbin = i;
//   }
//   double max = (error->GetMaximum() > datahist->GetMaximum()) ? error->GetMaximum() : datahist->GetMaximum();
//   double heightratio = 15.;
//   if(hs->GetNhists() != 0) hs->GetXaxis()->SetRange(firstbin,lastbin+1);
//   if(hs->GetNhists() != 0) hs->GetYaxis()->SetTitle("Events");
//   if(hs->GetNhists() != 0) hs->SetMaximum(max*(1.0/heightratio + 1));

// }

void setXAxisTop(TH1* datahist, TH1* error, THStack* hs) {
  TAxis* xaxis = hs->GetXaxis();
  int lastbin = 0, firstbin = 1;
  for(int i = 0; i < datahist->GetXaxis()->GetNbins(); i++) {
    if(datahist->GetBinContent(i+1) != 0 || error->GetBinContent(i+1) != 0) lastbin = i+1;
    if(datahist->GetXaxis()->GetXmin() < 0 && firstbin == 1 && (datahist->GetBinContent(i+1) != 0 || error->GetBinContent(i+1) != 0)) firstbin = i;
  }
  xaxis->SetRange(firstbin, lastbin+1);
}

void setYAxisTop(TH1* datahist, TH1* error, double ratio, THStack* hs) {
  //  TAxis* yaxis = hs->GetYaxis();
  double max = (error->GetMaximum() > datahist->GetMaximum()) ? error->GetMaximum() : datahist->GetMaximum();
  hs->SetMaximum(max*(1.0/ratio + 1.0));
  //  yaxis->SetLimits(0, max*(1.0/ratio + 1.0));
}

void setXAxisBot(TH1* data_mc, TAxis* otheraxis, double ratio) {
  TAxis* xaxis = data_mc->GetXaxis();
  xaxis->SetRange(otheraxis->GetFirst(), otheraxis->GetLast());
  xaxis->SetLabelSize(xaxis->GetLabelSize()*ratio);
}

void setYAxisBot(TH1* data_mc, double ratio) {
  TAxis* yaxis = data_mc->GetYaxis();

  double divmin = 0.0, divmax = 2.99;
  double low=2.99, high=0.0, tmpval;
  for(int i = 0; i < data_mc->GetXaxis()->GetNbins(); i++) {
    tmpval = data_mc->GetBinContent(i+1);
    if(tmpval < 2.99 && tmpval > high) {high = tmpval;}
    if(tmpval > 0. && tmpval < low) {low = tmpval;}
  }
  double val = min(abs(1 / (high - 1.)), abs(1 / (1/low -1.)));
  double factor = 1.0;
  while(val > factor) {
    divmin = 1.0 - 1.0/factor;
    divmax = 1/divmin;
    factor *= 2.0;
  }
       
  yaxis->SetRangeUser(divmin,divmax);
  yaxis->SetLabelSize(yaxis->GetLabelSize()*ratio);

}
