//by Nicolas Tonon (DESY)

//--- LIST OF FUNCTIONS (for quick search) :
//--------------------------------------------
// Train_BDT

// Produce_Templates

// Draw_Templates

// Compare_TemplateShapes_Processes

//--------------------------------------------

#include "TopEFT_analysis.h"

// #include "TMVA/PyMethodBase.h" //PYTHON -- Keras interface
// #include "TMVA/MethodPyKeras.h" //PYTHON -- Keras interface
// #include "TMVA/HyperParameterOptimisation.h"
// #include "TMVA/VariableImportance.h"
// #include "TMVA/CrossValidation.h" //REMOVABLE -- needed for BDT optim

//To read weights from DNN trained with Keras (without TMVA) -- requires c++14, etc.
// #include "fdeep/fdeep.hpp" //REMOVABLE

#define MYDEBUG(msg) cout<<endl<<ITAL("-- DEBUG: " << __FILE__ << ":" << __LINE__ <<":")<<FRED(" " << msg  <<"")<<endl

using namespace std;





//---------------------------------------------------------------------------
// ####    ##    ##    ####    ########
//  ##     ###   ##     ##        ##
//  ##     ####  ##     ##        ##
//  ##     ## ## ##     ##        ##
//  ##     ##  ####     ##        ##
//  ##     ##   ###     ##        ##
// ####    ##    ##    ####       ##
//---------------------------------------------------------------------------

/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

//Overloaded constructor
TopEFT_analysis::TopEFT_analysis(vector<TString> thesamplelist, vector<TString> thesamplegroups, vector<TString> thesystlist, vector<TString> thesystTreelist, vector<TString> thechannellist, vector<TString> thevarlist, vector<TString> set_v_cut_name, vector<TString> set_v_cut_def, vector<bool> set_v_cut_IsUsedForBDT, vector<TString> set_v_add_var_names, TString theplotextension, double lumi, bool show_pulls, TString region, TString signal_process, TString classifier_name, TString DNN_type)
{
    //Canvas definition
    Load_Canvas_Style();

    TH1::SetDefaultSumw2();
    gStyle->SetErrorX(0.);

    nbins = 10; //default

	mkdir("outputs", 0777);
	mkdir("plots", 0777);

	stop_program = false;

	this->region = region;

    this->signal_process = signal_process;

	// this->is_blind = is_blind;

    sample_list.resize(thesamplelist.size());
    sample_groups.resize(thesamplelist.size());
	for(int i=0; i<thesamplelist.size(); i++)
	{
        sample_list[i] = thesamplelist[i];
        sample_groups[i] = thesamplegroups[i];
	}

	dir_ntuples = "./input_ntuples/";
	// cout<<"dir_ntuples : "<<dir_ntuples<<endl;

	//-- Get colors
	color_list.resize(sample_list.size());
	Get_Samples_Colors(color_list, sample_list, 0);

	this->classifier_name = classifier_name;
	if(classifier_name == "DNN")
	{
		if(DNN_type.Contains("TMVA", TString::kIgnoreCase)) {this->DNN_type = "TMVA";}
		else if(DNN_type.Contains("PyKeras", TString::kIgnoreCase)) {this->DNN_type = "PyKeras";}
		else if(DNN_type.Contains("Keras", TString::kIgnoreCase)) {this->DNN_type = "Keras";}
		else {cout<<BOLD(FRED("Wrong value of DNN_type !"))<<endl; stop_program = true;}
	}

    // t_name = "Tree";
    t_name = "result";

	Set_Luminosity(lumi, "2017");
	// if(use_2016_ntuples) {Set_Luminosity(35.862, "2016");} //Use 2016 lumi instead

	plot_extension = theplotextension;

	show_pulls_ratio = show_pulls;

	syst_list.resize(thesystlist.size());
	for(int i=0; i<thesystlist.size(); i++)
	{
		syst_list[i] = thesystlist[i];
	}
	if(syst_list.size() == 0 || syst_list[0] != "") {cout<<"ERROR : first element of 'syst_list' is not empty (<-> nominal event weight) ! If that's what you want, remove this protection !"<<endl; stop_program = true;}

	systTree_list.resize(thesystTreelist.size());
	for(int i=0; i<thesystTreelist.size(); i++)
	{
		systTree_list[i] = thesystTreelist[i];
	}
	if(systTree_list.size() == 0 || systTree_list[0] != "") {cout<<"ERROR : first element of 'systTree_list' is not empty (<-> nominal TTree) ! If that's what you want, remove this protection !"<<endl; stop_program = true;}

	channel_list.resize(thechannellist.size());
	for(int i=0; i<thechannellist.size(); i++)
	{
		channel_list[i] = thechannellist[i];
	}
	if(channel_list.size() == 0 || channel_list[0] != "") {cout<<"ERROR : first element of 'channel_list' is not empty (<-> no subcat.) or vector is empty ! If that's what you want, remove this protection !"<<endl; stop_program = true;}

	int nof_categories_activated = 0; //Make sure only 1 orthogonal cat. is activated at once
	for(int i=0; i<set_v_cut_name.size(); i++) //Region cuts vars (e.g. NJets)
	{
		if(set_v_cut_name[i].Contains("is_") && set_v_cut_def[i] == "==1") //Consider that this variable is an event category, encoded as Char_t
		{
			nof_categories_activated++;
		}

		v_cut_name.push_back(set_v_cut_name[i]);
		v_cut_def.push_back(set_v_cut_def[i]);
		v_cut_IsUsedForBDT.push_back(set_v_cut_IsUsedForBDT[i]);
		v_cut_float.push_back(-999);
		v_cut_char.push_back(0);

		//NOTE : it is a problem if a variable is present in more than 1 list, because it will cause SetBranchAddress conflicts (only the last SetBranchAddress to a branch will work)
		//---> If a variable is present in 2 lists, erase it from other lists !
		for(int ivar=0; ivar<thevarlist.size(); ivar++)
		{
			if(thevarlist[ivar].Contains("is_") )
			{
				cout<<BOLD(FRED("## Warning : categories should not been used as input/spectator variables ! Are you sure ? "))<<endl;
			}
			if(thevarlist[ivar] == set_v_cut_name[i])
			{
				cout<<FGRN("** Constructor")<<" : erased variable "<<thevarlist[ivar]<<" from vector thevarlist (possible conflict) !"<<endl;
				thevarlist.erase(thevarlist.begin() + ivar);
				ivar--; //modify index accordingly
			}

		}
		for(int ivar=0; ivar<set_v_add_var_names.size(); ivar++)
		{
			if(set_v_add_var_names[ivar].Contains("is_") )
			{
				cout<<BOLD(FRED("## Warning : categories should not been used as input/spectator variables ! Are you sure ? "))<<endl;
			}
			if(set_v_add_var_names[ivar] == set_v_cut_name[i])
			{
				cout<<FGRN("** Constructor")<<" : erased variable "<<set_v_add_var_names[ivar]<<" from vector set_v_add_var_names (possible conflict) !"<<endl;
				set_v_add_var_names.erase(set_v_add_var_names.begin() + ivar);
				ivar--; //modify index accordingly
			}
		}

		// cout<<"Cuts : name = "<<v_cut_name[i]<<" / def = "<<v_cut_def[i]<<endl;
	}
	for(int i=0; i<thevarlist.size(); i++) //TMVA vars
	{
		var_list.push_back(thevarlist[i]);
		var_list_floats.push_back(-999);

		for(int ivar=0; ivar<set_v_add_var_names.size(); ivar++)
		{
			if(set_v_add_var_names[ivar] == thevarlist[i])
			{
				cout<<FGRN("** Constructor")<<" : erased variable "<<set_v_add_var_names[ivar]<<" from vector set_v_add_var_names (possible conflict) !"<<endl;
				set_v_add_var_names.erase(set_v_add_var_names.begin() + ivar);
				ivar--; //modify index accordingly
			}
		}
	}

	for(int i=0; i<set_v_add_var_names.size(); i++) //Additional vars, only for CR plots
	{
		v_add_var_names.push_back(set_v_add_var_names[i]);
		v_add_var_floats.push_back(-999);
	}

	//Make sure that the "==" sign is written properly, or rewrite it
	for(int ivar=0; ivar<v_cut_name.size(); ivar++)
	{
		if( v_cut_def[ivar].Contains("=") && !v_cut_def[ivar].Contains("!") && !v_cut_def[ivar].Contains("==") && !v_cut_def[ivar].Contains("<") && !v_cut_def[ivar].Contains(">") )
		{
			v_cut_def[ivar] = "==" + Convert_Number_To_TString(Find_Number_In_TString(v_cut_def[ivar]));

			cout<<endl<<BOLD(FBLU("##################################"))<<endl;
			cout<<"--- Changed cut on "<<v_cut_name[ivar]<<" to: "<<v_cut_def[ivar]<<" ---"<<endl;
			cout<<BOLD(FBLU("##################################"))<<endl<<endl;
		}
	}

	// color_list.resize(thecolorlist.size());
	// for(int i=0; i<thecolorlist.size(); i++)
	// {
	// 	color_list[i] = thecolorlist[i];
	// }
	// if(use_custom_colorPalette) {Set_Custom_ColorPalette(v_custom_colors, color_list);}

	//Store the "cut name" that will be written as a suffix in the name of each output file
	this->filename_suffix = "";
	TString tmp = "";
	for(int ivar=0; ivar<v_cut_name.size(); ivar++)
	{
		if(v_cut_name[ivar].Contains("is_") ) {continue;} //No need to appear in filename
		else if(v_cut_name[ivar] == "nLightJets_Fwd40") {this->filename_suffix+= "_fwdCut"; continue;} //No need to appear in filename

		if(v_cut_def[ivar] != "")
		{
            if(!v_cut_def[ivar].Contains("&&") && !v_cut_def[ivar].Contains("||") ) //Single condition
            {
                tmp+= "_" + v_cut_name[ivar] + Convert_Sign_To_Word(v_cut_def[ivar]) + Convert_Number_To_TString(Find_Number_In_TString(v_cut_def[ivar]));
            }
            else if(v_cut_def[ivar].Contains("&&")) //Double '&&' condition
            {
                TString cut1 = Break_Cuts_In_Two(v_cut_def[ivar]).first, cut2 = Break_Cuts_In_Two(v_cut_def[ivar]).second;
                tmp = "_" + v_cut_name[ivar] + Convert_Sign_To_Word(cut1) + Convert_Number_To_TString(Find_Number_In_TString(cut1));
                tmp+= Convert_Sign_To_Word(cut2) + Convert_Number_To_TString(Find_Number_In_TString(cut2));
            }
            else if(v_cut_def[ivar].Contains("||") )
            {
                TString cut1 = Break_Cuts_In_Two(v_cut_def[ivar]).first, cut2 = Break_Cuts_In_Two(v_cut_def[ivar]).second;
                tmp = "_" + v_cut_name[ivar] + Convert_Sign_To_Word(cut1) + Convert_Number_To_TString(Find_Number_In_TString(cut1));
                tmp+= "OR" + Convert_Sign_To_Word(cut2) + Convert_Number_To_TString(Find_Number_In_TString(cut2));

            }

			this->filename_suffix+= tmp;
		}
	}

	cout<<endl<<endl<<BOLD(FBLU("[Region : "<<region<<"]"))<<endl<<endl<<endl;

    //-- Protections

    usleep(2000000); //Pause for 3s (in microsec)
}

TopEFT_analysis::~TopEFT_analysis()
{

}


/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////
/**
 * Compute the luminosity re-scaling factor (MC),  to be used thoughout the code
 * @param desired_luminosity [Value of the desired lumi in fb-1]
 */
void TopEFT_analysis::Set_Luminosity(double desired_luminosity, TString reference)
{
	if(reference == "2017") {ref_luminosity = 41.5;}
	else if(reference == "2016") {ref_luminosity = 35.862;} //Moriond 2017 lumi ref.
	assert(ref_luminosity > 0 && "Using wrong lumi reference -- FIX !"); //Make sure we use 2016 or 2017 as ref.

	this->luminosity_rescale = desired_luminosity / ref_luminosity;

	if(luminosity_rescale != 1)
	{
		cout<<endl<<BOLD(FBLU("##################################"))<<endl;
		cout<<"--- Using luminosity scale factor : "<<desired_luminosity<<" / "<<ref_luminosity<<" = "<<luminosity_rescale<<" ! ---"<<endl;
		cout<<BOLD(FBLU("##################################"))<<endl<<endl;
	}
}













//---------------------------------------------------------------------------
// ########    ########        ###       ####    ##    ##    ####    ##    ##     ######
//    ##       ##     ##      ## ##       ##     ###   ##     ##     ###   ##    ##    ##
//    ##       ##     ##     ##   ##      ##     ####  ##     ##     ####  ##    ##
//    ##       ########     ##     ##     ##     ## ## ##     ##     ## ## ##    ##   ####
//    ##       ##   ##      #########     ##     ##  ####     ##     ##  ####    ##    ##
//    ##       ##    ##     ##     ##     ##     ##   ###     ##     ##   ###    ##    ##
//    ##       ##     ##    ##     ##    ####    ##    ##    ####    ##    ##     ######
//---------------------------------------------------------------------------


/////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////

void TopEFT_analysis::Train_BDT(TString channel, bool write_ranking_info)
{
	//--- Options
	bool use_relative_weights = true; //if false, will use fabs(weight)

//--------------------------------------------
	cout<<endl<<BOLD(FYEL("##################################"))<<endl;
	cout<<FYEL("--- TRAINING ---")<<endl;
	cout<<BOLD(FYEL("##################################"))<<endl<<endl;

	if(use_relative_weights) {cout<<"-- Using "<<BOLD(FGRN("*RELATIVE weights*"))<<" --"<<endl<<endl<<endl;}
	else {cout<<"-- Using "<<BOLD(FGRN("*ABSOLUTE weights*"))<<" --"<<endl<<endl<<endl;}

	mkdir("weights", 0777);
	mkdir("weights/BDT", 0777);
	// mkdir("weights/DNN", 0777);
	// mkdir("weights/DNN/TMVA", 0777);
	// mkdir("weights/DNN/PyKeras", 0777);
	// mkdir("weights/DNN/Keras", 0777);

	usleep(1000000); //Pause for 1s (in microsec)

//--------------------------------
//  ####  #    # #####  ####
// #    # #    #   #   #
// #      #    #   #    ####
// #      #    #   #        #
// #    # #    #   #   #    #
//  ####   ####    #    ####
//--------------------------------------------

	//---Apply additional cuts on the signal and background samples (can be different)
	TCut mycuts = "";
	TCut mycutb = "";
	TString tmp = "";

	//--- CHOOSE TRAINING EVENTS <--> cut on corresponding category
	TString cat_tmp = "";
	// cat_tmp = Get_Category_Boolean_Name(nLep_cat, region, analysis_type, "", scheme);

	//Even if ask templates in the SR, need to use training (looser) category for training !
	// if(cat_tmp.Contains("_SR") )
	// {
	// 	int i = cat_tmp.Index("_SR"); //Find index of substring
	// 	cat_tmp.Remove(i); //Remove substring
	// }
    // tmp+= cat_tmp + "==1";

	//--- Define additionnal cuts
	for(int ivar=0; ivar<v_cut_name.size(); ivar++)
	{
		if(v_cut_def[ivar] != "")
		{
			tmp+= " && ";

			if(!v_cut_def[ivar].Contains("&&") && !v_cut_def[ivar].Contains("||")) {tmp+= v_cut_name[ivar] + v_cut_def[ivar];} //If cut contains only 1 condition
			else if(v_cut_def[ivar].Contains("&&") && v_cut_def[ivar].Contains("||")) {cout<<BOLD(FRED("ERROR ! Wrong cut definition !"))<<endl;}
			else if(v_cut_def[ivar].Contains("&&") )//If '&&' in the cut, break it in 2
			{
				tmp+= v_cut_name[ivar] + Break_Cuts_In_Two(v_cut_def[ivar]).first;
				tmp+= " && ";
				tmp+= v_cut_name[ivar] + Break_Cuts_In_Two(v_cut_def[ivar]).second;
			}
			else if(v_cut_def[ivar].Contains("||") )//If '||' in the cut, break it in 2
			{
				tmp+= v_cut_name[ivar] + Break_Cuts_In_Two(v_cut_def[ivar]).first;
				tmp+= " || ";
				tmp+= v_cut_name[ivar] + Break_Cuts_In_Two(v_cut_def[ivar]).second;
			}
		}
	}

	bool split_by_leptonChan = false;
	if(split_by_leptonChan && (channel != "all" && channel != ""))
	{
		if(channel == "uuu" || channel == "uu")	{mycuts = "Channel==0"; mycutb = "Channel==0";}
		else if(channel == "uue" || channel == "ue") {mycuts = "Channel==1"; mycutb = "Channel==1";}
		else if(channel == "eeu" || channel == "ee") {mycuts = "Channel==2"; mycutb = "Channel==2";}
		else if(channel == "eee") {mycuts = "Channel==3"; mycutb = "Channel==3";}
		else {cout << "WARNING : wrong channel name while training " << endl;}
	}

	cout<<"-- Will apply the following cut(s) : "<<BOLD(FGRN(""<<tmp<<""))<<endl<<endl<<endl<<endl;
	usleep(2000000); //Pause for 2s (in microsec)

	if(tmp != "") {mycuts+= tmp; mycutb+= tmp;}

	//--------------------------------------------
	//---------------------------------------------------------------
    // This loads the TMVA libraries
    TMVA::Tools::Instance();

	//Allows to bypass a protection in TMVA::Transplot_extensionionHandler, cf. description in source file:
	// if there are too many input variables, the creation of correlations plots blows up memory and basically kills the TMVA execution --> avoid above critical number (which can be user defined)
	(TMVA::gConfig().GetVariablePlotting()).fMaxNumOfAllowedVariablesForScatterPlots = 300;

	TString output_file_name = "outputs/" + classifier_name;
	if(channel != "") {output_file_name+= "_" + channel;}
	if(classifier_name == "DNN") {output_file_name+= "_" + DNN_type;}
	output_file_name+= "__" + signal_process;
	output_file_name+= this->filename_suffix + ".root";

	TFile* output_file = TFile::Open( output_file_name, "RECREATE" );

	// Create the factory object
	// TMVA::Factory* factory = new TMVA::Factory(type.Data(), output_file, "!V:!Silent:Color:DrawProgressBar:AnalysisType=Classification" );
	TString weights_dir = "weights";
	TMVA::DataLoader *dataloader = new TMVA::DataLoader(weights_dir); //If no TString given in arg, will store weights in : default/weights/...

	//--- Could modify here the name of dir. containing the BDT weights (default = "weights")
	//By setting it to "", weight files will be stored directly at the path given to dataloader
	//Complete path for weight files is : [path_given_toDataloader]/[fWeightFileDir]
	//Apparently, TMVAGui can't handle nested repos in path given to dataloader... so split path in 2 here
	TMVA::gConfig().GetIONames().fWeightFileDir = "BDT";
	if(classifier_name == "DNN" && DNN_type == "TMVA") {TMVA::gConfig().GetIONames().fWeightFileDir = "DNN/TMVA";}

//--------------------------------------------
 // #    #   ##   #####  #   ##   #####  #      ######  ####
 // #    #  #  #  #    # #  #  #  #    # #      #      #
 // #    # #    # #    # # #    # #####  #      #####   ####
 // #    # ###### #####  # ###### #    # #      #           #
 //  #  #  #    # #   #  # #    # #    # #      #      #    #
 //   ##   #    # #    # # #    # #####  ###### ######  ####
//--------------------------------------------

	// Define the input variables that shall be used for the MVA training
	for(int i=0; i<var_list.size(); i++)
	{
		dataloader->AddVariable(var_list[i].Data(), 'F');
	}

	//Choose if the cut variables are used in BDT or not
	//Spectator vars are not used for training/evalution, but possible to check their correlations, etc.
	for(int i=0; i<v_cut_name.size(); i++)
	{
		// cout<<"Is "<<v_cut_name[i]<<" used ? "<<(v_cut_IsUsedForBDT[i] && !v_cut_def[i].Contains("=="))<<endl;

		// if we ask "var == x", all the selected events will be equal to x, so can't use it as discriminant variable !
		if(v_cut_IsUsedForBDT[i] && !v_cut_def[i].Contains("==")) {dataloader->AddVariable(v_cut_name[i].Data(), 'F');}
		// else {dataloader->AddSpectator(v_cut_name[i].Data(), v_cut_name[i].Data(), 'F');}
	}
	for(int i=0; i<v_add_var_names.size(); i++)
	{
		// dataloader->AddSpectator(v_add_var_names[i].Data(), v_add_var_names[i].Data(), 'F');
	}

	double nEvents_sig = 0;
	double nEvents_bkg = 0;


//--------------------------------------------
 //                          #
 //  ####  #  ####          #     #####  #    #  ####
 // #      # #    #        #      #    # #   #  #    #
 //  ####  # #            #       #####  ####   #
 //      # # #  ###      #        #    # #  #   #  ###
 // #    # # #    #     #         #    # #   #  #    #
 //  ####  #  ####     #          #####  #    #  ####
//--------------------------------------------
	//--- Only use few samples for training
	std::vector<TFile *> files_to_close;
	for(int isample=0; isample<sample_list.size(); isample++)
    {
		cout<<"-- Sample : "<<sample_list[isample]<<endl;

		TString samplename_tmp = sample_list[isample];

        // --- Register the training and test trees
        TString inputfile = dir_ntuples + sample_list[isample] + ".root";

//--------------------------------------------
// ##### ##### #####  ###### ######  ####
//   #     #   #    # #      #      #
//   #     #   #    # #####  #####   ####
//   #     #   #####  #      #           #
//   #     #   #   #  #      #      #    #
//   #     #   #    # ###### ######  ####
//--------------------------------------------

	    TFile *file_input = 0, *file_input_train = 0, *file_input_test = 0;
		TTree *tree = 0, *tree_train = 0, *tree_test = 0;

        file_input = TFile::Open(inputfile);
        if(!file_input) {cout<<BOLD(FRED(<<inputfile<<" not found!"))<<endl; continue;}
        files_to_close.push_back(file_input);
        tree = (TTree*) file_input->Get(t_name);
        if(tree==0) {cout<<BOLD(FRED("ERROR :"))<<" file "<<inputfile<<" --> *tree = 0 !"<<endl; continue;}
        else {cout<<endl<<FMAG("=== Opened file : ")<<inputfile<<endl<<endl;}

        // global event weights per tree (see below for setting event-wise weights)
		//NB : in tHq2016, different global weights were attributed to each sample !
		// ( see : https://github.com/stiegerb/cmgtools-lite/blob/80X_M17_tHqJan30/TTHAnalysis/python/plotter/tHq-multilepton/signal_mva/trainTHQMVA.py#L78-L98)
        Double_t signalWeight     = 1.0;
        Double_t backgroundWeight = 1.0;

    //-- Choose between absolute/relative weights for training
		if(samplename_tmp.Contains(signal_process) )
		{
            nEvents_sig+= tree->GetEntries(mycuts); dataloader->AddSignalTree(tree, signalWeight);

			if(use_relative_weights)
			{
                // TString weightExp = "weight";
                TString weightExp = "eventWeight";
				dataloader->SetSignalWeightExpression(weightExp);
				cout<<"Signal sample : "<<samplename_tmp<<" / Weight expression : "<<weightExp<<endl<<endl;
			}
			else
			{
                // TString weightExp = "fabs(weight)";
                TString weightExp = "fabs(eventWeight)";
				dataloader->SetSignalWeightExpression(weightExp);
				cout<<"Signal sample : "<<samplename_tmp<<" / Weight expression : "<<weightExp<<endl<<endl;
			}
		}
		else
		{
            nEvents_bkg+= tree->GetEntries(mycutb); dataloader->AddBackgroundTree(tree, backgroundWeight);

            if(use_relative_weights)
			{
                // TString weightExp = "weight";
                TString weightExp = "eventWeight";
				dataloader->SetBackgroundWeightExpression(weightExp);
				cout<<"Background sample : "<<samplename_tmp<<" / Weight expression : "<<weightExp<<endl;
			}
			else
			{
                // TString weightExp = "fabs(weight)";
                TString weightExp = "fabs(eventWeight)";
				dataloader->SetBackgroundWeightExpression(weightExp);
				cout<<"Background sample : "<<samplename_tmp<<" / Weight expression : "<<weightExp<<endl;
			}
		}
    }


//--------------------------------
// #####  #####  ###### #####    ##   #####  ######    ##### #####  ###### ######  ####
// #    # #    # #      #    #  #  #  #    # #           #   #    # #      #      #
// #    # #    # #####  #    # #    # #    # #####       #   #    # #####  #####   ####
// #####  #####  #      #####  ###### #####  #           #   #####  #      #           #
// #      #   #  #      #      #    # #   #  #           #   #   #  #      #      #    #
// #      #    # ###### #      #    # #    # ######      #   #    # ###### ######  ####
//--------------------------------------------

	if(mycuts != mycutb) {cout<<__LINE__<<FRED("PROBLEM : cuts are different for signal and background ! If this is normal, modify code -- Abort")<<endl; return;}

    // Tell the factory how to use the training and testing events

	// If nTraining_Events=nTesting_Events="0", half of the events in the tree are used for training, and the other half for testing
	//NB : ttH seem to use 80% of stat for training, 20% for testing. Kirill using 25K training and testing events for now
	//NB : when converting nEvents to TString, make sure to ask for sufficient precision !

	// float trainingEv_proportion = 0.5;
	float trainingEv_proportion = 0.7;

	//-- Choose dataset splitting
	TString nTraining_Events_sig = "", nTraining_Events_bkg = "", nTesting_Events_sig = "", nTesting_Events_bkg = "";

    int nmaxEv = 150000; //max nof events for train or test
    int nTrainEvSig = (nEvents_sig * trainingEv_proportion < nmaxEv) ? nEvents_sig * trainingEv_proportion : nmaxEv;
    int nTrainEvBkg = (nEvents_bkg * trainingEv_proportion < nmaxEv) ? nEvents_bkg * trainingEv_proportion : nmaxEv;
    int nTestEvSig = (nEvents_sig * (1-trainingEv_proportion) < nmaxEv) ? nEvents_sig * (1-trainingEv_proportion) : nmaxEv;
    int nTestEvBkg = (nEvents_bkg * (1-trainingEv_proportion) < nmaxEv) ? nEvents_bkg * (1-trainingEv_proportion) : nmaxEv;

    nTraining_Events_sig = Convert_Number_To_TString(nTrainEvSig, 12);
    nTraining_Events_bkg = Convert_Number_To_TString(nTrainEvBkg, 12);
    nTesting_Events_sig = Convert_Number_To_TString(nTestEvSig, 12);
    nTesting_Events_bkg = Convert_Number_To_TString(nTestEvBkg, 12);

	cout<<endl<<endl<<FBLU("===================================")<<endl;
	cout<<FBLU("-- Requesting "<<nTraining_Events_sig<<" Training events [SIGNAL]")<<endl;
	cout<<FBLU("-- Requesting "<<nTesting_Events_sig<<" Testing events [SIGNAL]")<<endl;
	cout<<FBLU("-- Requesting "<<nTraining_Events_bkg<<" Training events [BACKGROUND]")<<endl;
	cout<<FBLU("-- Requesting "<<nTesting_Events_bkg<<" Testing events [BACKGROUND]")<<endl;
	cout<<FBLU("===================================")<<endl<<endl<<endl;

    dataloader->PrepareTrainingAndTestTree(mycuts, mycutb, "nTrain_Signal="+nTraining_Events_sig+":nTrain_Background="+nTraining_Events_bkg+":nTest_Signal="+nTesting_Events_sig+":nTest_Background="+nTesting_Events_bkg+":SplitMode=Random:!V");

	//-- for quick testing
	// dataloader->PrepareTrainingAndTestTree(mycuts, mycutb, "nTrain_Signal=10:nTrain_Background=10:nTest_Signal=10:nTest_Background=10:SplitMode=Random:NormMode=NumEvents:!V");


	//--- Boosted Decision Trees -- Choose method
	TMVA::Factory *factory = new TMVA::Factory(classifier_name, output_file, "V:!Silent:Color:DrawProgressBar:Correlations=True:AnalysisType=Classification");

	// TString method_title = channel + this->filename_suffix; //So that the output weights are labelled differently for each channel
	TString method_title = ""; //So that the output weights are labelled differently for each channel
	if(channel != "") {method_title = channel;}
	else {method_title = "all";}
	method_title+= "_" + region;
	method_title+= "__" + signal_process;

//--------------------------------------------
//  ####  #####  ##### #  ####  #    #  ####     #    # ###### ##### #    #  ####  #####
// #    # #    #   #   # #    # ##   # #         ##  ## #        #   #    # #    # #    #
// #    # #    #   #   # #    # # #  #  ####     # ## # #####    #   ###### #    # #    #
// #    # #####    #   # #    # #  # #      #    #    # #        #   #    # #    # #    #
// #    # #        #   # #    # #   ## #    #    #    # #        #   #    # #    # #    #
//  ####  #        #   #  ####  #    #  ####     #    # ######   #   #    #  ####  #####
//--------------------------------------------
    TString method_options = "";

    //ttH2017
    method_options= "!H:!V:NTrees=200:BoostType=Grad:Shrinkage=0.10:!UseBaggedGrad:nCuts=200:nEventsMin=100:NNodesMax=5:MaxDepth=8:NegWeightTreatment=PairNegWeightsGlobal:CreateMVAPdfs:DoBoostMonitor=True";


//--------------------------------------------
 // ##### #####    ##   # #    #       ##### ######  ####  #####       ###### #    #   ##   #
 //   #   #    #  #  #  # ##   #         #   #      #        #         #      #    #  #  #  #
 //   #   #    # #    # # # #  #         #   #####   ####    #         #####  #    # #    # #
 //   #   #####  ###### # #  # #         #   #           #   #         #      #    # ###### #
 //   #   #   #  #    # # #   ##         #   #      #    #   #         #       #  #  #    # #
 //   #   #    # #    # # #    #         #   ######  ####    #         ######   ##   #    # ######
//--------------------------------------------

	if(classifier_name == "BDT") {factory->BookMethod(dataloader, TMVA::Types::kBDT, method_title, method_options);} //Book BDT
	else if(DNN_type == "TMVA") {factory->BookMethod(dataloader, TMVA::Types::kDNN, method_title, method_options);}

	output_file->cd();

	mkdir("outputs/Rankings", 0777); //Dir. containing variable ranking infos
	mkdir("outputs/ROCS", 0777); //Dir. containing variable ranking infos

	TString ranking_file_path = "outputs/Rankings/rank_"+classifier_name+"_"+region+".txt";

	if(write_ranking_info) cout<<endl<<endl<<endl<<FBLU("NB : Temporarily redirecting standard output to file '"<<ranking_file_path<<"' in order to save Ranking Info !!")<<endl<<endl<<endl;

	std::ofstream out("ranking_info_tmp.txt"); //Temporary name
	std::streambuf *coutbuf = std::cout.rdbuf(); //save old buf
	if(write_ranking_info) std::cout.rdbuf(out.rdbuf()); //redirect std::cout to text file --> Ranking info will be saved !

    // Train MVAs using the set of training events
    factory->TrainAllMethods();

	if(write_ranking_info) std::cout.rdbuf(coutbuf); //reset to standard output again

	//-- NB : Test & Evaluation recap in the output files
    factory->TestAllMethods(); // ---- Evaluate all MVAs using the set of test events
    factory->EvaluateAllMethods(); // ----- Evaluate and compare performance of all configured MVAs

	//Could retrieve ROC graph directly
	// TMultiGraph* rocgraph = f.GetROCCurveAsMultiGraph("<datasetname>");

    // --------------------------------------------------------------
    // Save the output
    output_file->Close();
    std::cout << "==> Wrote root file: " << output_file->GetName() << std::endl;
    std::cout << "==> TMVA is done!" << std::endl;

	if(write_ranking_info)
	{
		MoveFile("./ranking_info_tmp.txt", ranking_file_path);
		Extract_Ranking_Info(ranking_file_path, channel); //Extract only ranking info from TMVA output
	}
	else {system("rm ./ranking_info_tmp.txt");} //Else remove the temporary ranking file

	for(unsigned int i=0; i<files_to_close.size(); i++) {files_to_close[i]->Close(); delete files_to_close[i];}

	delete dataloader; dataloader = NULL;
	delete factory; factory = NULL;
	output_file->Close(); output_file = NULL;

	return;
}
































//---------------------------------------------------------------------------
//  ######  ########  ########    ###    ######## ########       ######## ######## ##     ## ########  ##          ###    ######## ########  ######
// ##    ## ##     ## ##         ## ##      ##    ##                ##    ##       ###   ### ##     ## ##         ## ##      ##    ##       ##    ##
// ##       ##     ## ##        ##   ##     ##    ##                ##    ##       #### #### ##     ## ##        ##   ##     ##    ##       ##
// ##       ########  ######   ##     ##    ##    ######            ##    ######   ## ### ## ########  ##       ##     ##    ##    ######    ######
// ##       ##   ##   ##       #########    ##    ##                ##    ##       ##     ## ##        ##       #########    ##    ##             ##
// ##    ## ##    ##  ##       ##     ##    ##    ##                ##    ##       ##     ## ##        ##       ##     ##    ##    ##       ##    ##
//  ######  ##     ## ######## ##     ##    ##    ########          ##    ######## ##     ## ##        ######## ##     ##    ##    ########  ######
//---------------------------------------------------------------------------


void TopEFT_analysis::Produce_Templates(TString template_name, bool makeHisto_inputVars)
{
//--------------------------------------------
    // bool xxx = xxx;
//--------------------------------------------

    cout<<endl<<BOLD(FYEL("##################################"))<<endl;
	if(makeHisto_inputVars) {cout<<FYEL("--- Producing Input variables histograms ---")<<endl;}
	else if(template_name == "") {cout<<FYEL("--- Producing "<<template_name<<" Templates ---")<<endl;}
	else {cout<<BOLD(FRED("--- ERROR : invalid arguments ! Exit !"))<<endl; cout<<"Valid template names are : ttbar / ttV / 2D / 2Dlin !"<<endl; return;}
	cout<<BOLD(FYEL("##################################"))<<endl<<endl;

	if(classifier_name != "BDT") {cout<<BOLD(FRED("Error : DNNs are not supported !"))<<endl; return;}

	//Don't make systematics shifted histos for input vars (too long)
	if(makeHisto_inputVars)
	{
		classifier_name = ""; //For naming conventions
		// syst_list.resize(1); syst_list[0] = ""; //Force Remove systematics
	}

//  ####  ###### ##### #    # #####
// #      #        #   #    # #    #
//  ####  #####    #   #    # #    #
//      # #        #   #    # #####
// #    # #        #   #    # #
//  ####  ######   #    ####  #

	TH1::SetDefaultSumw2();

	//Output file name
	//-- For BDT templates
	TString output_file_name = "outputs/Templates_" + classifier_name + template_name;
	output_file_name+= "_" + region + filename_suffix;
    output_file_name+= ".root";

	//-- For input vars
	if(makeHisto_inputVars)
	{
		output_file_name = "outputs/ControlHistograms";
		output_file_name+= "_" + region + filename_suffix +".root";
	}

    //Create output file
	TFile* file_output = 0;
    file_output = TFile::Open(output_file_name, "RECREATE");

    reader = new TMVA::Reader( "!Color:!Silent" );

	// Name & adress of local variables which carry the updated input values during the event loop
	// NB : the variable names MUST corresponds in name and type to those given in the weight file(s) used -- same order
	// NB : if booking 2 BDTs (e.g. template_name=="2Dlin", must make sure that they use the same input variables... or else, find some way to make it work in the code)
	for(int i=0; i<var_list.size(); i++)
	{
        reader->AddVariable(var_list[i].Data(), &var_list_floats[i]);
        //cout<<"Added variable "<<var_list[i]<<endl;
	}

	for(int i=0; i<v_cut_name.size(); i++)
	{
		if(v_cut_IsUsedForBDT[i] && !v_cut_def[i].Contains("=="))
		{
            reader->AddVariable(v_cut_name[i].Data(), &v_cut_float[i]);
		}
	}

	// --- Book the MVA methods (1 or 2, depending on template)
	TString dir = "weights/";
	dir+= classifier_name;

	TString MVA_method_name1 = "", MVA_method_name2 = "";
	TString weightfile = "";
	TString template_name_MVA = "";
	if(!makeHisto_inputVars)
	{
		if(template_name == "ttbar" || template_name == "ttV") //Book only 1 BDT
		{
			template_name_MVA = template_name + "_all";

			MVA_method_name1 = template_name_MVA + " method";
			weightfile = dir + "/" + classifier_name + "_" + template_name_MVA;
			weightfile+= "__" + signal_process + ".weights.xml";

			if(!Check_File_Existence(weightfile) ) {cout<<BOLD(FRED("Weight file "<<weightfile<<" not found ! Abort"))<<endl; return;}

			reader->BookMVA(MVA_method_name1, weightfile);
		}
		else if((template_name == "2Dlin" || template_name == "2D") && !makeHisto_inputVars) //Need to book 2 BDTs
		{
			template_name_MVA = "ttbar_all";
			MVA_method_name1 = template_name_MVA + " method";
			weightfile = dir + "/" + classifier_name + "_" + template_name_MVA + "__" + signal_process + ".weights.xml";
			if(!Check_File_Existence(weightfile) ) {cout<<BOLD(FRED("Weight file "<<weightfile<<" not found ! Abort"))<<endl; return;}
			reader1->BookMVA(MVA_method_name1, weightfile);

			template_name_MVA = "ttV_all";
			MVA_method_name2 = template_name_MVA + " method";
			weightfile = dir + "/" + classifier_name + "_" + template_name_MVA + "__" + signal_process + ".weights.xml";
			if(!Check_File_Existence(weightfile) ) {cout<<BOLD(FRED("Weight file "<<weightfile<<" not found ! Abort"))<<endl; return;}
			reader2->BookMVA(MVA_method_name2, weightfile);
		}

		// cout<<"MVA_method_name1 "<<MVA_method_name1<<endl;
		// cout<<"MVA_method_name2 "<<MVA_method_name2<<endl;
	}

	//Input TFile and TTree, called for each sample
	TFile* file_input;
	TTree* tree(0);

	//Template binning
	double xmin = -1, xmax = 1;
	nbins = 10;

	//Want to plot ALL selected variables
	vector<TString> total_var_list;
	if(makeHisto_inputVars)
	{
		for(int i=0; i<var_list.size(); i++)
		{
			total_var_list.push_back(var_list[i]);
		}
		for(int i=0; i<v_add_var_names.size(); i++)
		{
			total_var_list.push_back(v_add_var_names[i]);
		}
	}
	else
	{
		total_var_list.push_back(template_name);
	}
	vector<float> total_var_floats(total_var_list.size()); //NB : can not read/cut on BDT... (would conflict with input var floats ! Can not set address twice)

 // #    #      ##      #    #    #
 // ##  ##     #  #     #    ##   #
 // # ## #    #    #    #    # #  #
 // #    #    ######    #    #  # #
 // #    #    #    #    #    #   ##
 // #    #    #    #    #    #    #

 // #       ####   ####  #####   ####
 // #      #    # #    # #    # #
 // #      #    # #    # #    #  ####
 // #      #    # #    # #####       #
 // #      #    # #    # #      #    #
 // ######  ####   ####  #       ####

	cout<<endl<<endl<<"-- Ntuples dir. : "<<dir_ntuples<<endl<<endl;

	//SAMPLE LOOP
	for(int isample=0; isample<sample_list.size(); isample++)
	{
		cout<<endl<<endl<<UNDL(FBLU("Sample : "<<sample_list[isample]<<""))<<endl;

		//Open input TFile
		TString inputfile = dir_ntuples + sample_list[isample] + ".root";

		// cout<<"inputfile "<<inputfile<<endl;
		if(!Check_File_Existence(inputfile))
		{
			cout<<endl<<"File "<<inputfile<<FRED(" not found!")<<endl;
			continue;
		}

		file_input = TFile::Open(inputfile, "READ");

		//-- Loop on TTrees : first empty element corresponds to nominal TTree ; additional TTrees may correspond to JES/JER TTrees (defined in main)
		//NB : only nominal TTree contains systematic weights ; others only contain the nominal weight (but variables have different values)
		for(int itree=0; itree<systTree_list.size(); itree++)
		{
			tree = 0;
			if(systTree_list[itree] == "") {tree = (TTree*) file_input->Get(t_name);}
			else {tree = (TTree*) file_input->Get(systTree_list[itree]);}

			if(!tree)
			{
				cout<<BOLD(FRED("ERROR : tree '"<<systTree_list[itree]<<"' not found for sample : "<<sample_list[isample]<<" ! Skip !"))<<endl;
				continue; //Skip sample
			}


//   ##   #####  #####  #####  ######  ####   ####  ######  ####
//  #  #  #    # #    # #    # #      #      #      #      #
// #    # #    # #    # #    # #####   ####   ####  #####   ####
// ###### #    # #    # #####  #           #      # #           #
// #    # #    # #    # #   #  #      #    # #    # #      #    #
// #    # #####  #####  #    # ######  ####   ####  ######  ####

			//Disactivate all un-necessary branches ; below, activate only needed ones
			tree->SetBranchStatus("*", 0); //disable all branches, speed up
			// tree->SetBranchStatus("xxx", 1);

			if(makeHisto_inputVars)
			{
				for(int i=0; i<total_var_list.size(); i++)
				{
                    tree->SetBranchStatus(total_var_list[i], 1);
                    tree->SetBranchAddress(total_var_list[i], &total_var_floats[i]);
				}
			}
			else //Book input variables in same order as trained BDT
			{
                for(int i=0; i<var_list.size(); i++)
                {
                    tree->SetBranchStatus(var_list[i], 1);
                    tree->SetBranchAddress(var_list[i], &var_list_floats[i]);
                    // cout<<"Activate var '"<<var_list[i]<<"'"<<endl;
                }
			}

			for(int i=0; i<v_cut_name.size(); i++)
			{
				tree->SetBranchStatus(v_cut_name[i], 1);
				if(v_cut_name[i].Contains("is_") ) //Categories are encoded into Char_t, not float
				{
					tree->SetBranchAddress(v_cut_name[i], &v_cut_char[i]);
				}
				else //All others are floats
				{
					tree->SetBranchAddress(v_cut_name[i], &v_cut_float[i]);
				}
			}

			//--- Cut on relevant event selection (e.g. 3l SR, ttZ CR, etc.) -- stored as Char_t
			Char_t is_goodCategory; //Categ. of event
			// TString cat_name = Get_Category_Boolean_Name(nLep_cat, region, analysis_type, sample_list[isample], scheme);
			// tree->SetBranchStatus(cat_name, 1);
			// tree->SetBranchAddress(cat_name, &is_goodCategory);
			// cout<<"Categ <=> "<<cat_name<<endl;

			//--- Cut on relevant categorization (lepton flavour, btagging, charge)
			float channel, nMediumBJets, lepCharge;
			// tree->SetBranchStatus("channel", 1);
			// tree->SetBranchAddress("channel", &channel);
			// tree->SetBranchStatus("nMediumBJets", 1);
			// tree->SetBranchAddress("nMediumBJets", &nMediumBJets);
			// tree->SetBranchStatus("lepCharge", 1);
			// tree->SetBranchAddress("lepCharge", &lepCharge);
	//--- Event weights
			float weight; float weight_SF; //Stored separately
			float SMcoupling_SF, SMcoupling_weight, mc_weight_originalValue;
            float chargeLeadingLep;
			tree->SetBranchStatus("eventWeight", 1);
			tree->SetBranchAddress("eventWeight", &weight);
			// tree->SetBranchStatus("mc_weight_originalValue", 1);
			// tree->SetBranchAddress("mc_weight_originalValue", &mc_weight_originalValue);

            //Reserve 1 float for each systematic weight
			vector<Float_t> v_float_systWeights(syst_list.size());
			for(int isyst=0; isyst<syst_list.size(); isyst++)
			{
				//-- Protections : not all syst weights apply to all samples, etc.
				if(sample_list[isample] == "DATA" || sample_list[isample] == "QFlip") {break;}
				else if(systTree_list[itree] != "") {break;} //Syst event weights only stored in nominal TTree
				else if((syst_list[isyst].Contains("FR_") ) && !sample_list[isample].Contains("Fake") ) {continue;}
				else if(sample_list[isample].Contains("Fake") && !syst_list[isyst].Contains("FR_") && syst_list[isyst] != "") {continue;}
                else if(syst_list[isyst].Contains("thu_shape") || syst_list[isyst].Contains("Clos") ) {continue;} //these weights are computed within this func

				if(syst_list[isyst] != "") {tree->SetBranchStatus(syst_list[isyst], 1); tree->SetBranchAddress(syst_list[isyst], &v_float_systWeights[isyst]);} //Nominal weight already set, don't redo it
			}

			//Reserve memory for 1 TH1F* per category, per systematic
			vector<vector<vector<TH1F*>>> v3_histo_chan_syst_var(channel_list.size());
			vector<vector<vector<TH2F*>>> v3_histo_chan_syst_var2D(channel_list.size());

			for(int ichan=0; ichan<channel_list.size(); ichan++)
			{
				// if((channel_list.size() > 1 && channel_list[ichan] == "") || sample_list[isample] == "DATA" || sample_list[isample] == "QFlip" || systTree_list[itree] != "") {v3_histo_chan_syst_var[ichan].resize(1);} //Cases for which we only need to store the nominal weight
				if(sample_list[isample] == "DATA" || sample_list[isample] == "QFlip" || systTree_list[itree] != "")
				{
					v3_histo_chan_syst_var[ichan].resize(1); //Cases for which we only need to store the nominal weight
					if(template_name == "2D") {v3_histo_chan_syst_var2D[ichan].resize(1);}
				}
				else //Subcategories -> 1 histo for nominal + 1 histo per systematic
				{
					v3_histo_chan_syst_var[ichan].resize(syst_list.size());
					if(template_name == "2D") {v3_histo_chan_syst_var2D[ichan].resize(syst_list.size());}
				}

				//Init histos
				for(int isyst=0; isyst<v3_histo_chan_syst_var[ichan].size(); isyst++)
				{
					v3_histo_chan_syst_var[ichan][isyst].resize(total_var_list.size());

					if(template_name == "2D")
					{
						v3_histo_chan_syst_var2D[ichan][isyst].resize(1);
					}

					for(int ivar=0; ivar<total_var_list.size(); ivar++)
					{
						// if(makeHisto_inputVars && !Get_Variable_Range(total_var_list[ivar], nbins, xmin, xmax)) {cout<<FRED("Unknown variable name : "<<total_var_list[ivar]<<"! (add in function 'Get_Variable_Range()')")<<endl; continue;} //Get binning for this variable (if not template) //FIXME

						v3_histo_chan_syst_var[ichan][isyst][ivar] = new TH1F("", "", nbins, xmin, xmax);
						if(template_name == "2D") {v3_histo_chan_syst_var2D[ichan][isyst][0] = new TH2F("", "", 10, -1, 1, 10, -1, 1);}
					}
				} //syst
			} //chan

			// cout<<endl<< "--- "<<sample_list[isample]<<" : Processing: " << tree->GetEntries() << " events" << std::endl;


// ###### #    # ###### #    # #####    #       ####   ####  #####
// #      #    # #      ##   #   #      #      #    # #    # #    #
// #####  #    # #####  # #  #   #      #      #    # #    # #    #
// #      #    # #      #  # #   #      #      #    # #    # #####
// #       #  #  #      #   ##   #      #      #    # #    # #
// ######   ##   ###### #    #   #      ######  ####   ####  #

			cout<<"* Tree '"<<systTree_list[itree]<<"' :"<<endl;

			// int nentries = 10;
			int nentries = tree->GetEntries();

			// if(writeTemplate_forAllCouplingPoints) {nentries = 100;}

			float total_nentries = total_var_list.size()*nentries;
			cout<<"Will process : "<<total_var_list.size()<<" vars * "<<nentries<<" = "<<setprecision(9)<<total_nentries<<" events...."<<endl<<endl;

			//Draw progress bar
			// Int_t ibar = 0; //progress bar
			// TMVA::Timer timer(total_nentries, "", true);
			// TMVA::gConfig().SetDrawProgressBar(1);

			for(int ientry=0; ientry<nentries; ientry++)
			{
				// cout<<"ientry "<<ientry<<endl;

				if(!makeHisto_inputVars && ientry%20000==0) {cout<<ITAL(""<<ientry<<" / "<<nentries<<"")<<endl;}
				// else if(makeHisto_inputVars)
				// {
				// 	ibar++;
				// 	if(ibar%20000==0) {timer.DrawProgressBar(ibar, "test");}
				// }

				weight_SF = 1;
				SMcoupling_SF = 1;

				//Reset all vectors reading inputs to 0
				// std::fill(var_list_floats.begin(), var_list_floats.end(), 0);
				// std::fill(var_list_floats_ttbar.begin(), var_list_floats_ttbar.end(), 0);
				// std::fill(var_list_floats_ttV.begin(), var_list_floats_ttV.end(), 0);

				tree->GetEntry(ientry);

				// if(sample_list[isample] == "tZq") {cout<<"weight "<<weight<<endl;}
				if(isnan(weight) || isinf(weight))
				{
					cout<<BOLD(FRED("* Found event with weight = "<<weight<<" ; remove it..."))<<endl; continue;
				}

				//--- Cut on category value
				if(!is_goodCategory) {continue;}

//---- APPLY CUTS HERE (defined in main)  ----
				bool pass_all_cuts = true;
				for(int icut=0; icut<v_cut_name.size(); icut++)
				{
					if(v_cut_def[icut] == "") {continue;}

					//Categories are encoded into Char_t. Convert them to float for code automation
					if(v_cut_name[icut].Contains("is_") ) {v_cut_float[icut] = (float) v_cut_char[icut];}
					// cout<<"Cut : name="<<v_cut_name[icut]<<" / def="<<v_cut_def[icut]<<" / value="<<v_cut_float[icut]<<" / pass ? "<<Is_Event_Passing_Cut(v_cut_def[icut], v_cut_float[icut])<<endl;
					if(!Is_Event_Passing_Cut(v_cut_def[icut], v_cut_float[icut])) {pass_all_cuts = false; break;}
				}
				if(!pass_all_cuts) {continue;}
	//--------------------------------------------

				//Get MVA value to make template
				double mva_value1 = -9, mva_value2 = -9;

                if(!makeHisto_inputVars)
                {
                    mva_value1 = reader->EvaluateMVA(MVA_method_name1);
                }
                else
                {

                }

				//Get relevant binning
				float xValue_tmp = -1;

				// cout<<"//--------------------------------------------"<<endl;
				// cout<<"xValue_tmp = "<<xValue_tmp<<endl;
				// cout<<"mva_value1 = "<<mva_value1<<endl;
				// cout<<"mva_value2 = "<<mva_value2<<endl;

				//-- Fill histos for all subcategories
				for(int ichan=0; ichan<channel_list.size(); ichan++)
				{
					for(int isyst=0; isyst<syst_list.size(); isyst++)
					{
						double weight_tmp = 0; //Fill histo with this weight ; manipulate differently depending on syst

						// cout<<"-- Channel "<<channel_list[ichan]<<" / syst "<<syst_list[isyst]<<endl;

						if(syst_list[isyst] != "")
						{
							weight_tmp = v_float_systWeights[isyst];
						}
						else {weight_tmp = weight;} //Nominal (no syst)

						// cout<<"v_float_systWeights[isyst] "<<v_float_systWeights[isyst]<<endl;

						if(isnan(weight_tmp) || isinf(weight_tmp))
						{
							cout<<BOLD(FRED("* Found event with syst. weight = "<<weight_tmp<<" ; remove it..."))<<endl;
							cout<<"(Channel "<<channel_list[ichan]<<" / syst "<<syst_list[isyst]<<")"<<endl;
							continue;
						}

						//Printout weight
                        // if(sample_list[isample] == "ZZ" && syst_list[isyst].Contains("PU") && channel_list[ichan] == "")
						// cout<<"v3_histo_chan_syst_var["<<channel_list[ichan]<<"]["<<syst_list[isyst]<<"]+= "<<weight_tmp<<" => Integral "<<v3_histo_chan_syst_var[ichan][isyst]->Integral()<<endl;

						if(makeHisto_inputVars)
						{
							for(int ivar=0; ivar<total_var_list.size(); ivar++)
							{
								//Special variables, adress already used for other purpose
								if(total_var_list[ivar] == "nMediumBJets") {total_var_floats[ivar] = nMediumBJets;}
								else if(total_var_list[ivar] == "lepCharge") {total_var_floats[ivar] = lepCharge;}
								else if(total_var_list[ivar] == "channel") {total_var_floats[ivar] = channel;}

								Fill_TH1F_UnderOverflow(v3_histo_chan_syst_var[ichan][isyst][ivar], total_var_floats[ivar], weight_tmp);
							}
						}
						else
						{
                            if(template_name == "2D") {v3_histo_chan_syst_var2D[ichan][isyst][0]->Fill(mva_value1, mva_value2, weight_tmp);}
                            else {Fill_TH1F_UnderOverflow(v3_histo_chan_syst_var[ichan][isyst][0], xValue_tmp, weight_tmp);}
						}
					} //syst loop

					if(channel_list[ichan] != "") {break;} //subcategories are orthogonal ; if already found, can break subcat. loop
				} //subcat/chan loop

			} //end TTree entries loop
//--------------------------------------------

// #    # #  ####  #####  ####     #    #   ##   #    # # #####
// #    # # #        #   #    #    ##  ##  #  #  ##   # # #    #
// ###### #  ####    #   #    #    # ## # #    # # #  # # #    #
// #    # #      #   #   #    #    #    # ###### #  # # # #####
// #    # # #    #   #   #    #    #    # #    # #   ## # #
// #    # #  ####    #    ####     #    # #    # #    # # #

			for(int ichan=0; ichan<channel_list.size(); ichan++)
			{
				for(int isyst=0; isyst<syst_list.size(); isyst++)
				{
					for(int ivar=0; ivar<total_var_list.size(); ivar++)
					{
						// cout<<"chan "<<channel_list[ichan]<<"syst "<<syst_list[isyst]<<endl;

						if(sample_list[isample] != "DATA" && sample_list[isample] != "Fakes" && sample_list[isample] != "QFlip")
						{
							//Luminosity rescaling
							if(template_name == "2D") {v3_histo_chan_syst_var2D[ichan][isyst][ivar]->Scale(luminosity_rescale);}
							else {v3_histo_chan_syst_var[ichan][isyst][ivar]->Scale(luminosity_rescale);}
						} //MC
					} //Var
				} //syst
			} //ichan


// #    # #####  # ##### ######
// #    # #    # #   #   #
// #    # #    # #   #   #####
// # ## # #####  #   #   #
// ##  ## #   #  #   #   #
// #    # #    # #   #   ######

			// --- Write histograms
			TString samplename = sample_list[isample];
			if(sample_list[isample] == "DATA") {samplename = "data_obs";}

			for(int ichan=0; ichan<channel_list.size(); ichan++)
			{
				// cout<<"channel "<<channel_list[ichan]<<endl;

				for(int isyst=0; isyst<syst_list.size(); isyst++)
				{
					// cout<<"isyst "<<isyst<<endl;

					for(int ivar=0; ivar<total_var_list.size(); ivar++)
					{
						TString output_histo_name;
						if(makeHisto_inputVars)
						{
							output_histo_name = total_var_list[ivar] + "_" + region;
							if(channel_list[ichan] != "") {output_histo_name+= "_" + channel_list[ichan];}
							output_histo_name+= "__" + samplename;
							if(syst_list[isyst] != "") {output_histo_name+= "__" + syst_list[isyst];}
							else if(systTree_list[itree] != "") {output_histo_name+= "__" + systTree_list[itree];}
						}
						else
						{
							output_histo_name = classifier_name + template_name + "__" + region;
							if(channel_list[ichan] != "") {output_histo_name+= "_" + channel_list[ichan];}
							output_histo_name+= "__" + samplename;
							if(syst_list[isyst] != "") {output_histo_name+= "__" + syst_list[isyst];}
							else if(systTree_list[itree] != "") {output_histo_name+= "__" + systTree_list[itree];}
						}

						file_output->cd();

						if(template_name == "2D") {v3_histo_chan_syst_var2D[ichan][isyst][ivar]->Write(output_histo_name);}
						else {v3_histo_chan_syst_var[ichan][isyst][ivar]->Write(output_histo_name);}

						delete v3_histo_chan_syst_var[ichan][isyst][ivar]; v3_histo_chan_syst_var[ichan][isyst][ivar] = NULL;
						if(template_name == "2D") {delete v3_histo_chan_syst_var[ichan][isyst][ivar]; v3_histo_chan_syst_var[ichan][isyst][ivar] = NULL;}
					} //var loop
				} //syst loop
			} //chan loop

			// cout<<"Done with "<<sample_list[isample]<<" sample"<<endl;

			tree->ResetBranchAddresses(); //Detach tree from local variables (safe)
			delete tree; tree = NULL;
		} //end tree loop

		file_input->Close(); file_input = NULL;
	} //end sample loop


 //  ####  #       ####   ####  ######
 // #    # #      #    # #      #
 // #      #      #    #  ####  #####
 // #      #      #    #      # #
 // #    # #      #    # #    # #
 //  ####  ######  ####   ####  ######

	cout<<endl<<FYEL("==> Created root file: ")<<file_output->GetName()<<endl;
	cout<<FYEL("containing the "<<classifier_name<<" templates as histograms for : all samples / all channels")<<endl<<endl;

	file_output->Close(); file_output = NULL;

    if(template_name == "2Dlin" || template_name == "2D")
	{
		delete reader1; reader1 = NULL;
		delete reader2; reader2 = NULL;
	}
    else {delete reader; reader = NULL;}

	return;
}













//-----------------------------------------------------------------------------------------
// ########  ########     ###    ##      ##
// ##     ## ##     ##   ## ##   ##  ##  ##
// ##     ## ##     ##  ##   ##  ##  ##  ##
// ##     ## ########  ##     ## ##  ##  ##
// ##     ## ##   ##   ######### ##  ##  ##
// ##     ## ##    ##  ##     ## ##  ##  ##
// ########  ##     ## ##     ##  ###  ###

// ######## ######## ##     ## ########  ##          ###    ######## ########  ######
//    ##    ##       ###   ### ##     ## ##         ## ##      ##    ##       ##    ##
//    ##    ##       #### #### ##     ## ##        ##   ##     ##    ##       ##
//    ##    ######   ## ### ## ########  ##       ##     ##    ##    ######    ######
//    ##    ##       ##     ## ##        ##       #########    ##    ##             ##
//    ##    ##       ##     ## ##        ##       ##     ##    ##    ##       ##    ##
//    ##    ######## ##     ## ##        ######## ##     ##    ##    ########  ######
//-----------------------------------------------------------------------------------------

void TopEFT_analysis::Draw_Templates(bool drawInputVars, TString channel, TString template_name, bool prefit, bool use_combine_file)
{
//--------------------------------------------
	bool doNot_stack_signal = false;

	bool draw_errors = true; //true <-> superimpose error bands on plot/ratio plot

	bool draw_logarithm = false;
//--------------------------------------------

	cout<<endl<<BOLD(FYEL("##################################"))<<endl;
	if(drawInputVars) {cout<<FYEL("--- Producing Input Variables Plots / channel : "<<channel<<" ---")<<endl;}
	else if(template_name == "") {cout<<FYEL("--- Producing "<<template_name<<" Template Plots / channel : "<<channel<<" ---")<<endl;}
	else {cout<<FRED("--- ERROR : invalid args !")<<endl;}
	cout<<BOLD(FYEL("##################################"))<<endl<<endl;

	if(drawInputVars)
	{
		classifier_name = ""; //For naming conventions
		use_combine_file = false;
		if(drawInputVars && !prefit) {cout<<"Error ! Can not draw postfit input vars yet !"<<endl; return;}
		if(template_name == "categ" && !prefit) {cout<<"Can not plot yields per subcategory using the Combine output file ! Will plot [PREFIT] instead of [POSTFIT] !"<<endl; prefit = true;}
	}

//  ####  ###### ##### #    # #####
// #      #        #   #    # #    #
//  ####  #####    #   #    # #    #
//      # #        #   #    # #####
// #    # #        #   #    # #
//  ####  ######   #    ####  #

	//Can use 2 different files :
	//- the files containing the template histograms, produced with this code (-> only prefit plots)
	//- or, better, the file produced by Combine from the templates : contains the prefit distributions with total errors, and the postfit distribution
	//If want postfit plots, must use the Combine file. If want prefit plots, can use both of them (NB : errors will be different)

	//Get input TFile
	if(!prefit)
	{
		use_combine_file = true;
	}
	if(drawInputVars && use_combine_file)
	{
		cout<<"-- Setting 'use_combine_file = false' !"<<endl;
		use_combine_file = false;
	}

	TString input_name = "";

	if(use_combine_file)
	{
		input_name = "./outputs/fitDiagnostics_";
		input_name+= classifier_name + template_name + "_" + region + filename_suffix;
		if(classifier_name == "DNN") {input_name+= "_" + DNN_type;}
		input_name+= ".root";

		if(!Check_File_Existence(input_name))
		{
			input_name = "./outputs/fitDiagnostics.root"; //Try another name
			if(!Check_File_Existence(input_name))
			{
				cout<<FBLU("-- NB : File ")<<input_name<<FBLU(" (<-> file produced by COMBINE) not found !")<<endl;
				if(!prefit) {cout<<FRED("=> Can not produce postfit plots ! Abort !")<<endl; return;}

				use_combine_file = false;

				if(drawInputVars) //Input vars
				{
					input_name = "outputs/ControlHistograms_" + region + filename_suffix + ".root";
				}
				else //Templates
				{
					input_name = "outputs/Templates_" + classifier_name + template_name + "_" + region + filename_suffix;
					if(classifier_name == "DNN") {input_name+= "_" + DNN_type;}
					input_name+= ".root";
				}

				if(!Check_File_Existence(input_name))
				{
					cout<<FRED("File "<<input_name<<" (<-> file containing prefit templates) not found ! Did you specify the region/background ? Abort")<<endl;
					return;
				}
				else {cout<<FBLU("--> Using file ")<<input_name<<FBLU(" instead (NB : only stat. error will be included)")<<endl;}
			}
			else {cout<<FBLU("--> Using Combine output file : ")<<input_name<<FBLU(" (NB : total error included)")<<endl; use_combine_file = true;}
		}
		else {cout<<FBLU("--> Using Combine output file : ")<<input_name<<FBLU(" (NB : total error included)")<<endl; use_combine_file = true;}
	}
	else
	{
		if(drawInputVars) //Input vars
		{
			input_name = "outputs/ControlHistograms_" + region + filename_suffix + ".root";
		}
		else //Templates
		{
			input_name = "outputs/Templates_" + classifier_name + template_name + "_" + region + filename_suffix;
			if(classifier_name == "DNN") {input_name+= "_" + DNN_type;}
			input_name+= ".root";
		}

		if(!Check_File_Existence(input_name))
		{
			cout<<FRED("File "<<input_name<<" (<-> file containing prefit templates) not found ! Did you specify the region/background ? Abort")<<endl;
			return;
		}
	}

	cout<<endl<<endl<<endl;
	usleep(1000000); //Pause for 1s (in microsec)

	//Input file containing histos
	TFile* file_input = 0;
	file_input = TFile::Open(input_name);

	//Need to rescale signal to fitted signal strength manually, and add its error in quadrature in each bin (verify)
	double sigStrength = 0;
	double sigStrength_Error = 0;
	if(!prefit)
	{
		TTree* t_postfit = (TTree*) file_input->Get("tree_fit_sb");
		t_postfit->SetBranchAddress("r", &sigStrength);
		t_postfit->SetBranchAddress("rErr", &sigStrength_Error);
		t_postfit->GetEntry(0); //Only 1 entry = fit results
		delete t_postfit; t_postfit = NULL;
	}

	//Want to plot ALL selected variables
	vector<TString> total_var_list;
	if(drawInputVars)
	{
		// for(int i=0; i<v_cut_name.size(); i++)
		// {
		// 	if(v_cut_name[i].Contains("is_")) {continue;} //Don't care about plotting the categories
		//
		// 	total_var_list.push_back(v_cut_name[i]);
		// }
		for(int i=0; i<var_list.size(); i++)
		{
			// if(!v_var_tmp->at(i).Contains("mem") || var_list[i].Contains("e-")) {continue;}

			total_var_list.push_back(var_list[i]);
		}
		for(int i=0; i<v_add_var_names.size(); i++)
		{
			total_var_list.push_back(v_add_var_names[i]);
		}
	}
	else
	{
		total_var_list.push_back(template_name);
	}


// #       ####   ####  #####   ####
// #      #    # #    # #    # #
// #      #    # #    # #    #  ####
// #      #    # #    # #####       #
// #      #    # #    # #      #    #
// ######  ####   ####  #       ####

	for(int ivar=0; ivar<total_var_list.size(); ivar++)
	{
		cout<<endl<<FBLU("* Variable : "<<total_var_list[ivar]<<" ")<<endl<<endl;

		//TH1F* to retrieve distributions
		TH1F* h_tmp = 0; //Tmp storing histo

		TH1F* h_thq = 0; //Store tHq shape
		TH1F* h_thw = 0; //Store tHW shape
		TH1F* h_fcnc = 0; //Store FCNC shape
		TH1F* h_fcnc2 = 0; //Store FCNC shape
		TH1F* h_sum_data = 0; //Will store data histogram
		vector<TH1F*> v_MC_histo; //Will store all MC histograms (1 TH1F* per MC sample)

		TGraphAsymmErrors* g_data = 0; //If using Combine file, data are stored in TGAE
		TGraphAsymmErrors* g_tmp = 0; //Tmp storing graph

		vector<TString> MC_samples_legend; //List the MC samples which are actually used (to get correct legend)

		//-- Init error vectors
		double x, y, errory_low, errory_high;

		vector<double> v_eyl, v_eyh, v_exl, v_exh, v_x, v_y; //Contain the systematic errors (used to create the TGraphError)
		int nofbins=-1;

		vector<float> v_yield_sig, v_yield_bkg;

		//Combine output : all histos are given for subcategories --> Need to sum them all
		for(int ichan=0; ichan<channel_list.size(); ichan++)
		{
			//If using my own template file, there is already a "summed categories" versions of the histograms
			if(channel_list[ichan] != channel)
			{
				if(use_combine_file) {if(channel != "") {continue;} } //In combine file, to get inclusive plot, must sum all subcategories
				else {continue;}
			}

			//Combine file : histos stored in subdirs -- define dir name
			TString dir_hist = "";
			if(prefit) {dir_hist = "shapes_prefit/";}
			else {dir_hist = "shapes_fit_s/";}
			dir_hist+= classifier_name + template_name + "_" + region;
			if(channel_list[ichan] != "") {dir_hist+= "_" + channel_list[ichan];} //for combine file
			dir_hist+= "/";
			if(use_combine_file && !file_input->GetDirectory(dir_hist)) {cout<<FRED("Directory "<<dir_hist<<" not found ! Skip !")<<endl; continue;}

// #    #  ####
// ##  ## #    #
// # ## # #
// #    # #
// #    # #    #
// #    #  ####

			//--- Retrieve all MC samples
			int nof_skipped_samples = 0; //Get sample index right

			vector<bool> v_isSkippedSample(sample_list.size()); //Get sample index right (some samples are skipped)

			for(int isample = 0; isample < sample_list.size(); isample++)
			{
				int index_MC_sample = isample - nof_skipped_samples; //Sample index, but not counting data/skipped sample

				//In Combine, some individual contributions are merged as "Rares"/"EWK", etc.
				//If using Combine file, change the names of the samples we look for, and look only once for histogram of each "group"
				TString samplename = sample_list[isample];
				if(use_combine_file)
				{
					if(isample > 0 && sample_groups[isample] == sample_groups[isample-1]) {v_isSkippedSample[isample] = true; nof_skipped_samples++; continue;} //if same group as previous sample
					else {samplename = sample_groups[isample];}
				}

				//Protections, special cases
				if(sample_list[isample].Contains("DATA") ) {v_isSkippedSample[isample] = true; nof_skipped_samples++; continue;}

				// cout<<endl<<UNDL(FBLU("-- Sample : "<<sample_list[isample]<<" : "))<<endl;

				h_tmp = 0;

				TString histo_name = "";
				if(use_combine_file) {histo_name = dir_hist + samplename;}
				else
				{
					histo_name = classifier_name + total_var_list[ivar] + "_" + region;
					if(channel != "") {histo_name+= "_" + channel;}
					histo_name+= + "__" + samplename;
				}

				if(use_combine_file && !file_input->GetDirectory(dir_hist)->GetListOfKeys()->Contains(samplename) ) {cout<<ITAL("Histogram '"<<histo_name<<"' : not found ! Skip...")<<endl; v_isSkippedSample[isample] = true; nof_skipped_samples++; continue;}
				else if(!use_combine_file && !file_input->GetListOfKeys()->Contains(histo_name) ) {cout<<ITAL("Histogram '"<<histo_name<<"' : not found ! Skip...")<<endl; v_isSkippedSample[isample] = true; nof_skipped_samples++; continue;}

				h_tmp = (TH1F*) file_input->Get(histo_name);
				// cout<<"histo_name "<<histo_name<<endl;
	            // cout<<"h_tmp->Integral() = "<<h_tmp->Integral()<<endl;

				if(draw_errors)
				{
					//Initialize error vectors (only once at start)
					if(nofbins == -1) //if not yet init, get histo parameters
					{
						nofbins = h_tmp->GetNbinsX();
						for(int ibin=0; ibin<nofbins; ibin++)
						{
							v_eyl.push_back(0); v_eyh.push_back(0);
							v_exl.push_back(h_tmp->GetXaxis()->GetBinWidth(ibin+1) / 2); v_exh.push_back(h_tmp->GetXaxis()->GetBinWidth(ibin+1) / 2);
							v_x.push_back( (h_tmp->GetXaxis()->GetBinLowEdge(nofbins+1) - h_tmp->GetXaxis()->GetBinLowEdge(1) ) * ((ibin+1 - 0.5)/nofbins) + h_tmp->GetXaxis()->GetBinLowEdge(1));
							v_y.push_back(0);
						}
					}

					//Increment errors
					for(int ibin=0; ibin<nofbins; ibin++) //Start at bin 1
					{
						//For the processes which will not be stacked (FCNC, ...), don't increment the y-values of the errors
						if(doNot_stack_signal && (samplename.Contains("tHq") || samplename.Contains("tHW")) ) {break;} //Don't stack signal
						if(samplename.Contains("FCNC") ) {break;} //Don't stack FCNC signal

						// NOTE : for postfit, the bin error accounts for all systematics !
						//If using Combine output file (from MLF), bin error contains total error. Else if using template file directly, just stat. error
						v_eyl[ibin]+= pow(h_tmp->GetBinError(ibin+1), 2);
						v_eyh[ibin]+= pow(h_tmp->GetBinError(ibin+1), 2);

						v_y[ibin]+= h_tmp->GetBinContent(ibin+1); //This vector is used to know where to draw the error zone on plot (= on top of stack)

						// if(ibin != 4) {continue;} //cout only 1 bin
						// cout<<"x = "<<v_x[ibin]<<endl;    cout<<", y = "<<v_y[ibin]<<endl;    cout<<", eyl = "<<v_eyl[ibin]<<endl;    cout<<", eyh = "<<v_eyh[ibin]<<endl; //cout<<", exl = "<<v_exl[ibin]<<endl;    cout<<", exh = "<<v_exh[ibin]<<endl;
					} //loop on bins

					//-- NEW, draw all errors
					//--------------------------------------------
					if(!use_combine_file) //In Combine file, already accounted in binError
					{
						for(int itree=0; itree<systTree_list.size(); itree++)
						{
							for(int isyst=0; isyst<syst_list.size(); isyst++)
							{
								//-- Protections : not all syst weights apply to all samples, etc.
								// if(syst_list[isyst] != "" && systTree_list[itree] != "") {break;} //only nominal

								// cout<<"sample "<<sample_list[isample]<<" / channel "<<channel_list[ichan]<<" / syst "<<syst_list[isyst]<<endl;

								TH1F* histo_syst = 0; //Store the "systematic histograms"

								TString histo_name_syst = histo_name + "__" + syst_list[isyst];

								if(!file_input->GetListOfKeys()->Contains(histo_name_syst)) {continue;} //No error messages if systematics histos not found

								histo_syst = (TH1F*) file_input->Get(histo_name_syst);

								//Add up here the different errors (quadratically), for each bin separately
								for(int ibin=0; ibin<nofbins; ibin++)
								{
									if(histo_syst->GetBinContent(ibin+1) == 0) {continue;} //Some syst may be null, don't compute diff

									double tmp = 0;

									//For each systematic, compute (shifted-nominal), check the sign, and add quadratically to the corresponding bin error
									tmp = histo_syst->GetBinContent(ibin+1) - h_tmp->GetBinContent(ibin+1);

									if(tmp>0) {v_eyh[ibin]+= pow(tmp,2);}
									else if(tmp<0) {v_eyl[ibin]+= pow(tmp,2);}

									if(ibin > 0) {continue;} //cout only first bin
									// cout<<"//--------------------------------------------"<<endl;
									// cout<<"Sample "<<sample_list[isample]<<" / Syst "<<syst_list[isyst]<< " / chan "<<channel_list[ichan]<<endl;
									// cout<<"x = "<<v_x[ibin]<<endl;    cout<<", y = "<<v_y[ibin]<<endl;    cout<<", eyl = "<<v_eyl[ibin]<<endl;    cout<<", eyh = "<<v_eyh[ibin]<<endl; //cout<<", exl = "<<v_exl[ibin]<<endl;    cout<<", exh = "<<v_exh[ibin]<<endl;
									// cout<<"(nominal value = "<<h_tmp->GetBinContent(ibin+1)<<" - shifted value = "<<histo_syst->GetBinContent(ibin+1)<<") = "<<h_tmp->GetBinContent(ibin+1)-histo_syst->GetBinContent(ibin+1)<<endl;
								}

								delete histo_syst;
							} //end syst loop
						} //systTree_list loop
					} //--- systematics error loop
					//--------------------------------------------
				} //error condition

				if(!samplename.Contains("DATA") )
				{
					if(v_MC_histo.size() <=  index_MC_sample) {MC_samples_legend.push_back(samplename);}
					// MC_samples_legend.push_back(samplename); //Fill vector containing existing MC samples names
					// cout<<"ADD samplename "<<samplename<<endl;
					// cout<<"MC_samples_legend.size() "<<MC_samples_legend.size()<<endl;
				}

 //  ####   ####  #       ####  #####   ####
 // #    # #    # #      #    # #    # #
 // #      #    # #      #    # #    #  ####
 // #      #    # #      #    # #####       #
 // #    # #    # #      #    # #   #  #    #
 //  ####   ####  ######  ####  #    #  ####

				//Use color vector filled in main()
				h_tmp->SetFillStyle(1001);
				if(samplename == "Fakes") {h_tmp->SetFillStyle(3005);}
		        else if(samplename == "QFlip" ) {h_tmp->SetFillStyle(3006);}
		        else if(samplename.Contains("TTbar") || samplename.Contains("TTJet") )
				{
					// h_tmp->SetFillStyle(3005);
					if(samplename.Contains("Semi") ) {h_tmp->SetLineWidth(0);}
				}

				h_tmp->SetFillColor(color_list[isample]);
				h_tmp->SetLineColor(kBlack);

				if((doNot_stack_signal && (samplename.Contains("tHq") || samplename.Contains("tHW")) ) || samplename.Contains("FCNC")) //Superimpose BSM signal
				{
					h_tmp->SetFillColor(0);
					h_tmp->SetLineColor(color_list[isample]);
				}

				//Check color of previous *used* sample (up to 6)
				for(int k=1; k<6; k++)
				{
					if(isample - k >= 0)
					{
						if(v_isSkippedSample[isample-k]) {continue;}
						else if(color_list[isample] == color_list[isample-k]) {h_tmp->SetLineColor(color_list[isample]); break;}
					}
					else {break;}
				}

				// v_MC_histo.push_back((TH1F*) h_tmp->Clone());
				if(v_MC_histo.size() <=  index_MC_sample) {v_MC_histo.push_back((TH1F*) h_tmp->Clone());}
				else if(!v_MC_histo[index_MC_sample]) {v_MC_histo[index_MC_sample] = (TH1F*) h_tmp->Clone();} //For FakeEle and FakeMu
				else {v_MC_histo[index_MC_sample]->Add((TH1F*) h_tmp->Clone());}

				if(channel_list[ichan] == "")
				{
					if(v_yield_sig.size() == 0) {v_yield_sig.resize(h_tmp->GetNbinsX()); v_yield_bkg.resize(h_tmp->GetNbinsX());}

					for(int ibin=0; ibin<h_tmp->GetNbinsX(); ibin++)
					{
						if(sample_list[isample].Contains(signal_process) ) {v_yield_sig[ibin]+= h_tmp->GetBinContent(ibin+1);}
						else {v_yield_bkg[ibin]+= h_tmp->GetBinContent(ibin+1);}
					}
				}

				// cout<<"sample : "<<sample_list[isample]<<" / color = "<<color_list[isample]<<" fillstyle = "<<h_tmp->GetFillStyle()<<endl;
				// cout<<"index_MC_sample "<<index_MC_sample<<endl;
				// cout<<"v_MC_histo.size() "<<v_MC_histo.size()<<endl;
				// cout<<"MC_samples_legend.size() "<<MC_samples_legend.size()<<endl<<endl;

				delete h_tmp; h_tmp = 0;
			} //end sample loop
		} //subcat loop

		//Printout S/B for each bin
		// for(int ibin=0; ibin<v_yield_sig.size(); ibin++)
		// {
		// 	cout<<endl<<"ibin : S/B = "<<v_yield_sig[ibin]/v_yield_bkg[ibin]<<" // bkg = "<<v_yield_bkg[ibin]<<" // sig = "<<v_yield_sig[ibin]<<endl;
		// }

// #####    ##   #####   ##
// #    #  #  #    #    #  #
// #    # #    #   #   #    #
// #    # ######   #   ######
// #    # #    #   #   #    #
// #####  #    #   #   #    #

		//--- Retrieve DATA histo
		h_tmp = 0;
		TString histo_name = "";
		if(use_combine_file) {histo_name = "data";}
		else
		{
			histo_name = classifier_name + total_var_list[ivar] + "_" + region;
			if(channel != "") {histo_name+= "_" + channel;}
			histo_name+= "__data_obs";
		}

		if(use_combine_file)
		{
			for(int ichan=0; ichan<channel_list.size(); ichan++)
			{
				if(channel != "" && channel_list[ichan] != channel) {continue;}

				//Combine file : histos stored in subdirs -- define dir name
				TString dir_hist = "";
				if(prefit) {dir_hist = "shapes_prefit/";}
				else {dir_hist = "shapes_fit_s/";}
				dir_hist+= classifier_name + template_name + "_" + region;
				if(channel_list[ichan] != "") {dir_hist+= "_" + channel_list[ichan] + "/";} //for combine file
				if(!file_input->GetDirectory(dir_hist)) {cout<<ITAL("Directory "<<dir_hist<<" not found ! Skip !")<<endl; continue;}

				if(!file_input->GetDirectory(dir_hist)->GetListOfKeys()->Contains("data")) {cout<<FRED(""<<dir_hist<<"data : not found ! Skip...")<<endl; continue;}

				histo_name = dir_hist + "/data";
				// cout<<"histo_name "<<histo_name<<endl;
				g_tmp = (TGraphAsymmErrors*) file_input->Get(histo_name); //stored as TGraph

				//Remove X-axis error bars, not needed for plot
				for(int ipt=0; ipt<g_tmp->GetN(); ipt++)
				{
					g_tmp->SetPointEXhigh(ipt, 0);
					g_tmp->SetPointEXlow(ipt, 0);
				}

				if(!g_data) {g_data = (TGraphAsymmErrors*) g_tmp->Clone();}
				else //Need to sum TGraphs content by hand //not anymore, 1 channel only !
				{
					double x_tmp,y_tmp,errory_low_tmp,errory_high_tmp;
					for(int ipt=0; ipt<g_data->GetN(); ipt++)
					{
						g_data->GetPoint(ipt, x, y);
						errory_low = g_data->GetErrorYlow(ipt);
						errory_high = g_data->GetErrorYhigh(ipt);

						g_tmp->GetPoint(ipt, x_tmp, y_tmp);
						errory_low_tmp = g_tmp->GetErrorYlow(ipt);
						errory_high_tmp = g_tmp->GetErrorYhigh(ipt);

						double new_error_low = sqrt(errory_low*errory_low+errory_low_tmp*errory_low_tmp);
						double new_error_high = sqrt(errory_high_tmp*errory_high_tmp+errory_high_tmp*errory_high_tmp);
						g_data->SetPoint(ipt, x, y+y_tmp);
						g_data->SetPointError(ipt,0,0, new_error_low, new_error_high); //ok to add errors in quadrature ?

						// cout<<"ipt "<<ipt<<" / x1 "<<x<<" / y1 "<<y<<" / error1 "<<errory_low<<", "<<errory_high<<endl;
						// cout<<"ipt "<<ipt<<" / x2 "<<x_tmp<<" / y2 "<<y_tmp<<" / error2 "<<errory_low_tmp<<", "<<errory_high_tmp<<endl;
						// cout<<"=> y1+y2 = "<<y+y_tmp<<" / error = "<<new_error_low<<", "<<new_error_high<<endl;
					}
				}
			} //chan loop
		}
		else //If using template file made from this code
		{
			if(!file_input->GetListOfKeys()->Contains(histo_name)) {cout<<histo_name<<" : not found"<<endl;}
			else
			{
				h_tmp = (TH1F*) file_input->Get(histo_name);
				if(h_sum_data == 0) {h_sum_data = (TH1F*) h_tmp->Clone();}
				else {h_sum_data->Add((TH1F*) h_tmp->Clone());} //not needed anymore (1 channel only)

				delete h_tmp; h_tmp = NULL;
			}
		}

		bool data_notEmpty = true;
		// if(use_combine_file && !g_data) {cout<<endl<<BOLD(FRED("--- Empty data TGraph ! Exit !"))<<endl<<endl; return;}
		// if(!use_combine_file && !h_sum_data) {cout<<endl<<BOLD(FRED("--- Empty data histogram ! Exit !"))<<endl<<endl; return;}
		if(use_combine_file && !g_data) {cout<<endl<<BOLD(FRED("--- Empty data TGraph !"))<<endl<<endl; data_notEmpty = false;}
		if(!use_combine_file && !h_sum_data) {cout<<endl<<BOLD(FRED("--- Empty data histogram "<<histo_name<<" !"))<<endl<<endl; data_notEmpty = false;}

		//Make sure there are no negative bins
		if(data_notEmpty)
		{
			if(use_combine_file)
			{
				for(int ipt=0; ipt<g_data->GetN(); ipt++)
				{
					g_data->GetPoint(ipt, x, y);
					if(y<0) {g_data->SetPoint(ipt, x, 0); g_data->SetPointError(ipt,0,0,0,0);}
				}
			}
			else
			{
				for(int ibin = 1; ibin<h_sum_data->GetNbinsX()+1; ibin++)
				{
					if(h_sum_data->GetBinContent(ibin) < 0) {h_sum_data->SetBinContent(ibin, 0);}
				}
			}
			for(int k=0; k<v_MC_histo.size(); k++)
			{
				if(!v_MC_histo[k]) {continue;} //Fakes templates can be null
				for(int ibin=0; ibin<v_MC_histo[k]->GetNbinsX(); ibin++)
				{
					if(v_MC_histo[k]->GetBinContent(ibin) < 0) {v_MC_histo[k]->SetBinContent(ibin, 0);}
				}
			}
		}


 // # #    # #####  ###### #    #
 // # ##   # #    # #       #  #
 // # # #  # #    # #####    ##
 // # #  # # #    # #        ##
 // # #   ## #    # #       #  #
 // # #    # #####  ###### #    #

	//-- Get indices of particular samples, sum the others into 1 single histo (used for ratio subplot)
		TH1F* histo_total_MC = 0; //Sum of all MC samples

		//Indices of important samples, for specific treatment
		int index_tHq_sample = -99;
		int index_tHW_sample = -99;
		int index_NPL_sample = -99;
		int index_FCNC_sample = -99;
		int index_FCNC_sample2 = -99;

		// cout<<"v_MC_histo.size() "<<v_MC_histo.size()<<endl;
		// cout<<"MC_samples_legend.size() "<<MC_samples_legend.size()<<endl;

		//Merge all the MC nominal histograms (contained in v_MC_histo)
		for(int i=0; i<v_MC_histo.size(); i++)
		{
			if(!v_MC_histo[i]) {continue;} //Fakes templates may be null

			// cout<<"MC_samples_legend[i] "<<MC_samples_legend[i]<<endl;

			if(MC_samples_legend[i].Contains("tHq") )
			{
				if(index_tHq_sample<0) {index_tHq_sample = i;}
				if(!h_thq) {h_thq = (TH1F*) v_MC_histo[i]->Clone();}
				else {h_thq->Add((TH1F*) v_MC_histo[i]->Clone());}
				if(doNot_stack_signal) continue; //don't stack
			}
			else if(MC_samples_legend[i].Contains("tHW") )
			{
				if(index_tHW_sample<0) {index_tHW_sample = i;}
				if(!h_thw) {h_thw = (TH1F*) v_MC_histo[i]->Clone();}
				else {h_thw->Add((TH1F*) v_MC_histo[i]->Clone());}
				if(doNot_stack_signal) continue; //don't stack
			}
			else if(MC_samples_legend[i].Contains("FCNC") )
			{
				if(MC_samples_legend[i].Contains("tH_TT"))
				{
					if(index_FCNC_sample2<0) {index_FCNC_sample2 = i;}
					h_fcnc2 = (TH1F*) v_MC_histo[i]->Clone();
					continue; //don't stack FCNC signal
				}
				else if(MC_samples_legend[i].Contains("tH_ST"))
				{
					if(index_FCNC_sample<0) {index_FCNC_sample = i;}
					h_fcnc = (TH1F*) v_MC_histo[i]->Clone();
					continue; //don't stack FCNC signal
				}
			}

			// cout<<"Adding sample "<<MC_samples_legend[i]<<" to histo_total_MC"<<endl;

			if(!histo_total_MC) {histo_total_MC = (TH1F*) v_MC_histo[i]->Clone();}
			else {histo_total_MC->Add((TH1F*) v_MC_histo[i]->Clone());}
		}


// ####### #
//    #    #       ######  ####  ###### #    # #####
//    #    #       #      #    # #      ##   # #    #
//    #    #       #####  #      #####  # #  # #    #
//    #    #       #      #  ### #      #  # # #    #
//    #    #       #      #    # #      #   ## #    #
//    #    ####### ######  ####  ###### #    # #####

		bool use_diff_legend_layout = false;
		TLegend* qw = 0;
		if(use_diff_legend_layout)
		{
			// if(doNot_stack_signal) {qw = new TLegend(0.50,.70,0.99,0.88);}
			if(draw_logarithm)
			{
				qw = new TLegend(0.20,0.28,0.65,0.50);
			}
			else
			{
                if(doNot_stack_signal) {qw = new TLegend(0.53,.75,1.,0.90);}
                else {qw = new TLegend(0.62,.72,1.,0.90);}
			}
			qw->SetNColumns(3);
		}
		else
		{
			if(doNot_stack_signal) {qw = new TLegend(0.79,.60,1.,1.);}
			else
			{
				if(use_combine_file && data_notEmpty)
				{
					double x_tmp, y_tmp;
					g_data->GetPoint(0, x_tmp, y_tmp);
					g_data->GetPoint(g_data->GetN()-1, x, y);
					if(y > y_tmp) {qw = new TLegend(0.20,0.48,0.20+0.16,0.48+0.39);}
				}
				else if(!use_combine_file && data_notEmpty && h_sum_data->GetBinContent(h_sum_data->GetNbinsX()) > h_sum_data->GetBinContent(1)) {qw = new TLegend(0.20,0.48,0.20+0.16,0.48+0.39);}
				else {qw = new TLegend(.83,.60,0.99,0.99);}
			}
		}

		qw->SetLineColor(1);
		qw->SetTextSize(0.03);

		//--Data on top of legend
        if(use_combine_file && g_data != 0) {qw->AddEntry(g_data, "Data" , "ep");}
        else if(!use_combine_file && h_sum_data != 0) {qw->AddEntry(h_sum_data, "Data" , "ep");}
        else {cout<<__LINE__<<BOLD(FRED(" : null data !"))<<endl;}

		for(int i=0; i<v_MC_histo.size(); i++)
		{
			// cout<<"MC_samples_legend[i] "<<MC_samples_legend[i]<<endl;

			if(!v_MC_histo[i]) {continue;} //Fakes templates can be null

			//Merged with other samples in legend -- don't add these
			// if(MC_samples_legend[i].Contains("xxx")
	        // || MC_samples_legend[i] == "xxx"
	        // ) {continue;}

			if(MC_samples_legend[i] == "tHq" || MC_samples_legend[i] == "tHq_hww")
			{
				if(doNot_stack_signal)
				{
					h_thq->SetLineColor(color_list[i]);
					if(sample_list[0] == "DATA") {h_thq->SetLineColor(color_list[i+1]);}
					h_thq->SetFillColor(0);
					h_thq->SetLineWidth(4);
				}
				else
				{
					qw->AddEntry(v_MC_histo[i], "tHq(#kappa_{t}=-1)", "f");
				}
			}

            else if(MC_samples_legend[i].Contains("ttH")) {qw->AddEntry(v_MC_histo[i], "ttH", "f");}
            else if(MC_samples_legend[i].Contains("tZq")) {qw->AddEntry(v_MC_histo[i], "tZq", "f");}
            else if(MC_samples_legend[i].Contains("ttZ")) {qw->AddEntry(v_MC_histo[i], "ttZ", "f");}
			else if(MC_samples_legend[i].Contains("ttW") ) {qw->AddEntry(v_MC_histo[i], "t#bar{t}W", "f");}
			else if(MC_samples_legend[i] == "ttZ") {qw->AddEntry(v_MC_histo[i], "t#bar{t}Z", "f");}
			else if(MC_samples_legend[i] == "WZ" || MC_samples_legend[i] == "WZ_b") {qw->AddEntry(v_MC_histo[i], "EWK", "f");}
			else if(MC_samples_legend[i] == "Fakes") {qw->AddEntry(v_MC_histo[i], "Non-prompt", "f");}
			else if(MC_samples_legend[i] == "QFlip") {qw->AddEntry(v_MC_histo[i], "Flip", "f");}
			else if(MC_samples_legend[i].Contains("GammaConv") ) {qw->AddEntry(v_MC_histo[i], "#gamma-conv.", "f");}
		}


// ##### #    #  ####  #####   ##    ####  #    #
//   #   #    # #        #    #  #  #    # #   #
//   #   ######  ####    #   #    # #      ####
//   #   #    #      #   #   ###### #      #  #
//   #   #    # #    #   #   #    # #    # #   #
//   #   #    #  ####    #   #    #  ####  #    #

		THStack* stack_MC = new THStack;

		//Add legend entries -- iterate backwards, so that last histo stacked is on top of legend
		//Also add MC histograms to the THStack
		for(int i=v_MC_histo.size()-1; i>=0; i--)
		{
			if(!v_MC_histo[i]) {continue;} //Some templates may be null

			if(doNot_stack_signal)
			{
				//-- If want to use split signal samples
				// if(split_signals_byHiggsDecay && ((i>=index_tHq_sample && i<=index_tHq_sample+2) || ((i>=index_tHW_sample && i<=index_tHW_sample+2)) ) ) {continue;}
				// else if(!split_signals_byHiggsDecay && (i==index_tHq_sample || i==index_tHW_sample) ) {continue;} //don't stack signal

				//-- If want to use full signal samples
				if(i==index_tHq_sample || i==index_tHW_sample) {continue;}
				if(MC_samples_legend[i].Contains("tHq") || MC_samples_legend[i].Contains("tHW")) {continue;}
			}
			if(i==index_FCNC_sample || i==index_FCNC_sample2) {continue;} //don't stack FCNC signal

			stack_MC->Add(v_MC_histo[i]);
			// cout<<"Stacking sample "<<MC_samples_legend[i]<<" / integral "<<v_MC_histo[i]->Integral()<<endl;
		}

		//Set Yaxis maximum & minimum
		if(use_combine_file && data_notEmpty)
		{
			Long64_t locmax = TMath::LocMax(g_data->GetN(), g_data->GetY()); //the corresponding x value can be obtained with double xmax = gr->GetX()[locmax];
			double ymax = g_data->GetY()[locmax];

			if(ymax > stack_MC->GetMaximum() ) {stack_MC->SetMaximum(ymax*1.3);}
			else stack_MC->SetMaximum(stack_MC->GetMaximum()*1.5);
		}
		else if(!use_combine_file && data_notEmpty)
		{
			if(h_sum_data->GetMaximum() > stack_MC->GetMaximum() ) {stack_MC->SetMaximum(h_sum_data->GetMaximum()+0.3*h_sum_data->GetMaximum());}
			else {stack_MC->SetMaximum(stack_MC->GetMaximum()*1.5);}
		}

		stack_MC->SetMinimum(0.0001); //Remove '0' label

		if(draw_logarithm)
		{
			stack_MC->SetMinimum(0.5);
			stack_MC->SetMaximum(stack_MC->GetMaximum()*6);
		}


// #####  #####    ##   #    #
// #    # #    #  #  #  #    #
// #    # #    # #    # #    #
// #    # #####  ###### # ## #
// #    # #   #  #    # ##  ##
// #####  #    # #    # #    #

		//Canvas definition
		Load_Canvas_Style();
		TCanvas* c1 = new TCanvas("c1","c1", 1000, 800);
		// TCanvas* c1 = new TCanvas("c1","c1", 600, 800);
		c1->SetTopMargin(0.1);
		c1->SetBottomMargin(0.25);
		// c1->SetRightMargin(0.15);

		if(draw_logarithm) {c1->SetLogy();}

		//Draw stack
		stack_MC->Draw("hist");

		//Draw data
		if(data_notEmpty)
		{
			if(use_combine_file)
			{
				g_data->SetMarkerStyle(20);
				g_data->Draw("e0psame");
			}
			else
			{
				h_sum_data->SetMarkerStyle(20);
				h_sum_data->SetMinimum(0.) ;
				h_sum_data->Draw("e0psame");
			}
		}

		//Superimpose shape of signal
		if(doNot_stack_signal)
		{
			if(h_thq != 0) {h_thq->Draw("same hist");}
			if(h_thw != 0) {h_thw->Draw("same hist");}
		}

		qw->Draw("same"); //Draw legend


// ###### #####  #####   ####  #####   ####      ####  #####   ##    ####  #    #
// #      #    # #    # #    # #    # #         #        #    #  #  #    # #   #
// #####  #    # #    # #    # #    #  ####      ####    #   #    # #      ####
// #      #####  #####  #    # #####       #         #   #   ###### #      #  #
// #      #   #  #   #  #    # #   #  #    #    #    #   #   #    # #    # #   #
// ###### #    # #    #  ####  #    #  ####      ####    #   #    #  ####  #    #

		//-- Compute sqrt of quadratic errors
		if(draw_errors)
		{
			for(int ibin=0; ibin<nofbins; ibin++)
			{
				v_eyh[ibin] = pow(v_eyh[ibin], 0.5);
				v_eyl[ibin] = pow(v_eyl[ibin], 0.5);

				// if(ibin > 0) {continue;} //cout only first bin
				// cout<<"x = "<<v_x[ibin]<<endl;    cout<<", y = "<<v_y[ibin]<<endl;    cout<<", eyl = "<<v_eyl[ibin]<<endl;    cout<<", eyh = "<<v_eyh[ibin]<<endl; //cout<<", exl = "<<v_exl[ibin]<<endl;    cout<<", exh = "<<v_exh[ibin]<<endl;
			}
		}

		//Use pointers to vectors : need to give the adress of first element (all other elements can then be accessed iteratively)
		double* eyl = &v_eyl[0];
		double* eyh = &v_eyh[0];
		double* exl = &v_exl[0];
		double* exh = &v_exh[0];
		double* xx = &v_x[0];
		double* yy = &v_y[0];

		//Create TGraphAsymmErrors with the error vectors / (x,y) coordinates --> Can superimpose it on plot
		TGraphAsymmErrors* gr_error = 0;

		gr_error = new TGraphAsymmErrors(nofbins,xx,yy,exl,exh,eyl,eyh);
		gr_error->SetFillStyle(3002);
		gr_error->SetFillColor(kBlack);
		gr_error->Draw("e2 same"); //Superimposes the uncertainties on stack


// #####    ##   ##### #  ####
// #    #  #  #    #   # #    #
// #    # #    #   #   # #    #
// #####  ######   #   # #    #
// #   #  #    #   #   # #    #
// #    # #    #   #   #  ####

// #####  #       ####  #####
// #    # #      #    #   #
// #    # #      #    #   #
// #####  #      #    #   #
// #      #      #    #   #
// #      ######  ####    #

		//-- create subpad to plot ratio
		TPad *pad_ratio = new TPad("pad_ratio", "pad_ratio", 0.0, 0.0, 1.0, 1.0);
		pad_ratio->SetTopMargin(0.75);
		pad_ratio->SetFillColor(0);
		pad_ratio->SetFillStyle(0);
		pad_ratio->SetGridy(1);
		pad_ratio->Draw();
		pad_ratio->cd(0);

		if(use_combine_file && data_notEmpty) //Copy the content of the data graph into a TH1F (NB : symmetric errors...?)
		{
			if(!v_MC_histo[0]) {cout<<__LINE__<<FRED("Error : v_MC_histo[0] is null ! Abort")<<endl; return;}

	        h_sum_data = (TH1F*) v_MC_histo[0]->Clone(); //To clone binning of the MC histos
			h_sum_data->SetFillColor(kBlack);
			h_sum_data->SetLineColor(kBlack);
			// cout<<"h_sum_data->GetNbinsX() "<<h_sum_data->GetNbinsX()<<endl;

	        for(int ipt=0; ipt<g_data->GetN(); ipt++)
	        {
	            g_data->GetPoint(ipt, x, y);
	            double error = g_data->GetErrorY(ipt);

	            h_sum_data->SetBinContent(ipt+1, y);
	            h_sum_data->SetBinError(ipt+1, error);
	        }
		}

		TH1F* histo_ratio_data = 0;
		if(data_notEmpty)
		{
			histo_ratio_data = (TH1F*) h_sum_data->Clone();

			//debug printout
			// cout<<"h_sum_data->GetBinContent(5) "<<h_sum_data->GetBinContent(5)<<endl;
			// cout<<"h_sum_data->GetBinError(5) "<<h_sum_data->GetBinError(5)<<endl;
			// cout<<"histo_total_MC->GetBinContent(5) "<<histo_total_MC->GetBinContent(5)<<endl;
			// cout<<"histo_total_MC->GetBinError(5) "<<histo_total_MC->GetBinError(5)<<endl;

			if(!show_pulls_ratio)
			{
				//To get error bars correct in ratio plot, must only account for errors from data, not MC ! (MC error shown as gray bad)
				for(int ibin=1; ibin<histo_total_MC->GetNbinsX()+1; ibin++)
				{
					histo_total_MC->SetBinError(ibin, 0);
				}

				histo_ratio_data->Divide(histo_total_MC);
			} //Ratio
		 	else //--- Compute pull distrib
			{
				for(int ibin=1; ibin<histo_ratio_data->GetNbinsX()+1; ibin++)
				{
					//Add error on signal strength (since we rescale signal manually)
					// double bin_error_mu = v_MC_histo.at(index_tZq_sample)->GetBinError(ibin) * sig_strength_err;
					// cout<<"bin_error_mu = "<<bin_error_mu<<endl;

					double bin_error_mu = 0; //No sig strength uncert. for prefit ! //-- postfit -> ?

					//Quadratic sum of systs, stat error, and sig strength error
					double bin_error = pow(pow(histo_total_MC->GetBinError(ibin), 2) + pow(histo_ratio_data->GetBinError(ibin), 2) + pow(bin_error_mu, 2), 0.5);

					// if(ibin==1) {cout<<"Data = "<<histo_ratio_data->GetBinContent(1)<<" / Total MC = "<<histo_total_MC->GetBinContent(1)<<" / error = "<<bin_error<<endl;}

					if(!histo_total_MC->GetBinError(ibin)) {histo_ratio_data->SetBinContent(ibin,-99);} //Don't draw null markers
					else{histo_ratio_data->SetBinContent(ibin, (histo_ratio_data->GetBinContent(ibin) - histo_total_MC->GetBinContent(ibin)) / bin_error );}
				}
			}

			//debug printout
			// cout<<"histo_ratio_data->GetBinContent(5) "<<histo_ratio_data->GetBinContent(5)<<endl;
			// cout<<"histo_ratio_data->GetBinError(5) "<<histo_ratio_data->GetBinError(5)<<endl;

			//Don't draw null data
			for(int ibin=1; ibin<histo_ratio_data->GetNbinsX()+1; ibin++)
			{
				// cout<<"histo_ratio_data["<<ibin<<"] = "<<histo_ratio_data->GetBinContent(ibin)<<endl;

				if(std::isnan(histo_ratio_data->GetBinContent(ibin)) || std::isinf(histo_ratio_data->GetBinContent(ibin)) || histo_ratio_data->GetBinContent(ibin) == 0) {histo_ratio_data->SetBinContent(ibin, -99);}
			}
		}
		else {histo_ratio_data = (TH1F*) histo_total_MC->Clone();}

		if(show_pulls_ratio) {histo_ratio_data->GetYaxis()->SetTitle("Pulls");}
		else {histo_ratio_data->GetYaxis()->SetTitle("Data/MC");}
		histo_ratio_data->GetYaxis()->SetTickLength(0.);
		histo_ratio_data->GetXaxis()->SetTitleOffset(1);
		histo_ratio_data->GetYaxis()->SetTitleOffset(1.2);
		histo_ratio_data->GetYaxis()->SetLabelSize(0.048);
		histo_ratio_data->GetXaxis()->SetLabelFont(42);
		histo_ratio_data->GetYaxis()->SetLabelFont(42);
		histo_ratio_data->GetXaxis()->SetTitleFont(42);
		histo_ratio_data->GetYaxis()->SetTitleFont(42);
		histo_ratio_data->GetYaxis()->SetNdivisions(503); //grid draw on primary tick marks only
		histo_ratio_data->GetXaxis()->SetNdivisions(505);
		histo_ratio_data->GetYaxis()->SetTitleSize(0.06);
		histo_ratio_data->GetXaxis()->SetTickLength(0.04);
		histo_ratio_data->SetMarkerStyle(20);
		histo_ratio_data->SetMarkerSize(1.2); //changed from 1.4

		//NB : when using SetMaximum(), points above threshold are simply not drawn
		//So for ratio plot, even if point is above max but error compatible with 1, point/error bar not represented!
		if(show_pulls_ratio)
		{
			histo_ratio_data->SetMinimum(-2.99);
			histo_ratio_data->SetMaximum(2.99);
		}
		else
		{
			histo_ratio_data->SetMinimum(-0.2);
			histo_ratio_data->SetMaximum(2.2);
		}

		if(drawInputVars)
		{
			histo_ratio_data->GetXaxis()->SetTitle(total_var_list[ivar]);
		}
		else
		{
			if(template_name=="2D") {histo_ratio_data->GetXaxis()->SetTitle("BDT2D bin");}
			else if(template_name=="2Dlin") {histo_ratio_data->GetXaxis()->SetTitle("BDT bin");}
			else {histo_ratio_data->GetXaxis()->SetTitle(classifier_name+" (vs "+template_name + ")");}

			if(template_name == "categ") //Vertical text X labels (categories names)
			{
				histo_ratio_data->GetXaxis()->SetTitle("Categ.");
				histo_ratio_data->GetXaxis()->SetLabelSize(0.08);
				histo_ratio_data->GetXaxis()->SetLabelOffset(0.02);

				// int nx = 2;
				// if(nLep_cat == "2l")
				// {
				// 	const char *labels[5]  = {"ee", "e#mu bl", "e#mu bt", "#mu#mu bl", "#mu#mu bt"};
				// 	for(int i=1;i<=5;i++) {histo_ratio_data->GetXaxis()->SetBinLabel(i,labels[i-1]);}
				// }
			}
		}

		pad_ratio->cd(0);
		if(show_pulls_ratio) {histo_ratio_data->Draw("HIST P");} //Draw ratio points
		else {histo_ratio_data->Draw("E1X0 P");} //Draw ratio points ; E1 : perpendicular lines at end ; X0 : suppress x errors

// ###### #####  #####   ####  #####   ####     #####    ##   ##### #  ####
// #      #    # #    # #    # #    # #         #    #  #  #    #   # #    #
// #####  #    # #    # #    # #    #  ####     #    # #    #   #   # #    #
// #      #####  #####  #    # #####       #    #####  ######   #   # #    #
// #      #   #  #   #  #    # #   #  #    #    #   #  #    #   #   # #    #
// ###### #    # #    #  ####  #    #  ####     #    # #    #   #   #  ####

		TGraphAsymmErrors* gr_ratio_error = 0;
		if(draw_errors)
		{
			//Copy previous TGraphAsymmErrors, then modify it -> error TGraph for ratio plot
			TGraphAsymmErrors *thegraph_tmp;
			double *theErrorX_h;
			double *theErrorY_h;
			double *theErrorX_l;
			double *theErrorY_l;
			double *theY;
			double *theX;

			thegraph_tmp = (TGraphAsymmErrors*) gr_error->Clone();
			theErrorX_h = thegraph_tmp->GetEXhigh();
			theErrorY_h = thegraph_tmp->GetEYhigh();
			theErrorX_l = thegraph_tmp->GetEXlow();
			theErrorY_l = thegraph_tmp->GetEYlow();
			theY        = thegraph_tmp->GetY() ;
			theX        = thegraph_tmp->GetX() ;

			//Divide error --> ratio
			for(int i=0; i<thegraph_tmp->GetN(); i++)
			{
				theErrorY_l[i] = theErrorY_l[i]/theY[i];
				theErrorY_h[i] = theErrorY_h[i]/theY[i];
				theY[i]=1; //To center the filled area around "1"
			}

			gr_ratio_error = new TGraphAsymmErrors(thegraph_tmp->GetN(), theX , theY ,  theErrorX_l, theErrorX_h, theErrorY_l, theErrorY_h);
			gr_ratio_error->SetFillStyle(3002);
			gr_ratio_error->SetFillColor(kBlack);
			// gr_ratio_error->SetFillColor(kCyan);

			pad_ratio->cd(0);
			if(!show_pulls_ratio) {gr_ratio_error->Draw("e2 same");} //Draw error bands in ratio plot
		} //draw errors


//  ####   ####   ####  #    # ###### ##### #  ####   ####
// #    # #    # #      ##  ## #        #   # #    # #
// #      #    #  ####  # ## # #####    #   # #       ####
// #      #    #      # #    # #        #   # #           #
// #    # #    # #    # #    # #        #   # #    # #    #
//  ####   ####   ####  #    # ######   #   #  ####   ####

		//-- Draw ratio y-lines manually
		TH1F *h_line1 = 0;
		TH1F *h_line2 = 0;
		if(data_notEmpty)
		{
			h_line1 = new TH1F("","",this->nbins, h_sum_data->GetXaxis()->GetXmin(), h_sum_data->GetXaxis()->GetXmax());
			h_line2 = new TH1F("","",this->nbins, h_sum_data->GetXaxis()->GetXmin(), h_sum_data->GetXaxis()->GetXmax());
			// TH1F *h_line3 = new TH1F("","",this->nbins, h_sum_data->GetXaxis()->GetXmin(), h_sum_data->GetXaxis()->GetXmax());
			for(int ibin=1; ibin<this->nbins +1; ibin++)
			{
				if(show_pulls_ratio)
				{
					h_line1->SetBinContent(ibin, -1);
					h_line2->SetBinContent(ibin, 1);
				}
				else
				{
					h_line1->SetBinContent(ibin, 0.5);
					h_line2->SetBinContent(ibin, 1.5);
				}
			}
			h_line1->SetLineStyle(6);
			h_line2->SetLineStyle(6);
			h_line1->Draw("hist same");
			h_line2->Draw("hist same");
		}

		double xmax_stack = stack_MC->GetXaxis()->GetXmax();
		double xmin_stack = stack_MC->GetXaxis()->GetXmin();
		TString Y_label = "Events";
		if(data_notEmpty)
		{
			double xmax_data = h_sum_data->GetXaxis()->GetXmax();
			double xmin_data = h_sum_data->GetXaxis()->GetXmin();
			Y_label = "Events / " + Convert_Number_To_TString( (xmax_data - xmin_data) / h_sum_data->GetNbinsX(), 2); //Automatically get the Y label depending on binning
		}

		if(stack_MC!= 0)
		{
			stack_MC->GetXaxis()->SetLabelFont(42);
			stack_MC->GetYaxis()->SetLabelFont(42);
			stack_MC->GetYaxis()->SetTitleFont(42);
			stack_MC->GetYaxis()->SetTitleSize(0.06);
			stack_MC->GetYaxis()->SetTickLength(0.04);
			stack_MC->GetXaxis()->SetLabelSize(0.0);
			stack_MC->GetYaxis()->SetLabelSize(0.048);
			stack_MC->GetXaxis()->SetNdivisions(505);
			stack_MC->GetYaxis()->SetNdivisions(506);
			stack_MC->GetYaxis()->SetTitleOffset(1.2);
			stack_MC->GetYaxis()->SetTitle(Y_label);
		}

	//----------------
	// CAPTIONS //
	//----------------
	// -- using https://twiki.cern.ch/twiki/pub/CMS/Internal/FigGuidelines

        bool draw_cms_prelim_label = false;

		float l = c1->GetLeftMargin();
		float t = c1->GetTopMargin();

		TString cmsText = "CMS";
		TLatex latex;
		latex.SetNDC();
		latex.SetTextColor(kBlack);
		latex.SetTextFont(61);
		latex.SetTextAlign(11);
		latex.SetTextSize(0.06);
		if(draw_cms_prelim_label) {latex.DrawLatex(l + 0.01, 0.92, cmsText);}

		TString extraText = "Preliminary";
		latex.SetTextFont(52);
		latex.SetTextSize(0.05);
		if(draw_cms_prelim_label)
		{
            latex.DrawLatex(l + 0.12, 0.92, extraText);
            // latex.DrawLatex(l, 0.92, extraText);
		}

		float lumi = ref_luminosity * luminosity_rescale;
		TString lumi_13TeV = Convert_Number_To_TString(lumi);
		lumi_13TeV += " fb^{-1} (13 TeV)";

		latex.SetTextFont(42);
		latex.SetTextAlign(31);
		latex.SetTextSize(0.04);

		if(use_diff_legend_layout) {latex.DrawLatex(0.95, 0.92,lumi_13TeV);}
		else
		{
			// if(h_sum_data->GetBinContent(h_sum_data->GetNbinsX() ) > h_sum_data->GetBinContent(1) ) {latex.DrawLatex(0.95, 0.92,lumi_13TeV);}
			// else {latex.DrawLatex(0.78, 0.92,lumi_13TeV);}
			latex.DrawLatex(0.78, 0.92,lumi_13TeV);
		}

		//------------------
		//-- Channel info
		TLatex text2 ;
		text2.SetNDC();
		text2.SetTextAlign(13);
		text2.SetTextSize(0.045);
		text2.SetTextFont(42);

		TString info_data;
        info_data = "l^{#pm}l^{#pm}l^{#pm}";

		// if(h_sum_data->GetBinContent(h_sum_data->GetNbinsX() ) > h_sum_data->GetBinContent(1) ) {text2.DrawLatex(0.55,0.87,info_data);}
		// else {text2.DrawLatex(0.20,0.87,info_data);}
		text2.DrawLatex(0.23,0.86,info_data);


// #    # #####  # ##### ######     ####  #    # ##### #####  #    # #####
// #    # #    # #   #   #         #    # #    #   #   #    # #    #   #
// #    # #    # #   #   #####     #    # #    #   #   #    # #    #   #
// # ## # #####  #   #   #         #    # #    #   #   #####  #    #   #
// ##  ## #   #  #   #   #         #    # #    #   #   #      #    #   #
// #    # #    # #   #   ######     ####   ####    #   #       ####    #

		if(drawInputVars)
		{
			mkdir("plots", 0777);
			mkdir("plots/input_vars", 0777);
			mkdir("plots/input_vars/", 0777);
			mkdir( ("plots/input_vars/"+region).Data(), 0777);
		}
		else
		{
			mkdir("plots", 0777);
			mkdir("plots/templates", 0777);
			mkdir("plots/templates/", 0777);
			mkdir( ("plots/templates/"+region).Data(), 0777);
			if(prefit) {mkdir( ("plots/templates/"+region+"/prefit").Data(), 0777);}
			else {mkdir( ("plots/templates/"+region+"/postfit").Data(), 0777);}
		}

		//Output
		TString output_plot_name;

		if(drawInputVars)
		{
			output_plot_name = "plots/input_vars/" + region + "/" + total_var_list[ivar];
		}
		else
		{
			output_plot_name = "plots/templates/" + region;
			if(prefit) {output_plot_name+= "/prefit/";}
			else {output_plot_name+= "/postfit/";}
			output_plot_name+= classifier_name + template_name + "_template";
		}
		if(channel != "") {output_plot_name+= "_" + channel;}
		output_plot_name+= this->filename_suffix;
		if(draw_logarithm) {output_plot_name+= "_log";}
		output_plot_name+= this->plot_extension;

		c1->SaveAs(output_plot_name);

		if(data_notEmpty)
		{
			delete h_line1; h_line1 = NULL;
			delete h_line2; h_line2 = NULL;
		}

		if(draw_errors) {delete gr_error; delete gr_ratio_error; gr_error = NULL; gr_ratio_error = NULL;}
		delete pad_ratio; pad_ratio = NULL;

		delete c1; c1 = NULL;
		delete qw; qw = NULL;
		delete stack_MC; stack_MC = NULL;

		if(use_combine_file) {delete g_data; g_data = NULL;}
	} //Var loop

	file_input->Close();

	return;
}














//--------------------------------------------
//  ######   #######  ##     ## ########     ###    ########  ########
// ##    ## ##     ## ###   ### ##     ##   ## ##   ##     ## ##
// ##       ##     ## #### #### ##     ##  ##   ##  ##     ## ##
// ##       ##     ## ## ### ## ########  ##     ## ########  ######
// ##       ##     ## ##     ## ##        ######### ##   ##   ##
// ##    ## ##     ## ##     ## ##        ##     ## ##    ##  ##
//  ######   #######  ##     ## ##        ##     ## ##     ## ########

//  ######  ##     ##    ###    ########  ########  ######
// ##    ## ##     ##   ## ##   ##     ## ##       ##    ##
// ##       ##     ##  ##   ##  ##     ## ##       ##
//  ######  ######### ##     ## ########  ######    ######
//       ## ##     ## ######### ##        ##             ##
// ##    ## ##     ## ##     ## ##        ##       ##    ##
//  ######  ##     ## ##     ## ##        ########  ######
//--------------------------------------------

void TopEFT_analysis::Compare_TemplateShapes_Processes(TString template_name, TString channel)
{
	bool drawInputVars = true;

	bool normalize = true;

	if(drawInputVars)
	{
		template_name = "metEt"; //Can hardcode here another variable
		classifier_name = ""; //For naming convention
	}

//--------------------------------------------
	vector<TString> v_samples; vector<TString> v_groups; vector<int> v_colors;
	v_samples.push_back("tZq"); v_groups.push_back("tZq"); v_colors.push_back(kBlack);

    vector<TString> v_syst;
    v_syst.push_back("");
    // v_syst.push_back("JESUp");

//--------------------------------------------

	cout<<endl<<BOLD(FYEL("##################################"))<<endl;
	if(drawInputVars) {cout<<FYEL("--- Producing Input Vars Plots / channel : "<<channel<<" ---")<<endl;}
	// else if(template_name == "ttbar" || template_name == "ttV" || template_name == "2D" || template_name == "2Dlin" || template_name == "categ") {cout<<FYEL("--- Producing "<<template_name<<" Template Plots / channel : "<<channel<<" ---")<<endl;}
	// else {cout<<FRED("--- ERROR : invalid template_name value !")<<endl;}
	cout<<BOLD(FYEL("##################################"))<<endl<<endl;

//  ####  ###### ##### #    # #####
// #      #        #   #    # #    #
//  ####  #####    #   #    # #    #
//      # #        #   #    # #####
// #    # #        #   #    # #
//  ####  ######   #    ####  #

	//Get input TFile
	TString input_name;
	if(drawInputVars)
	{
		input_name = "outputs/ControlHistograms_" + region + filename_suffix + ".root";
	}
	else
	{
		input_name = "outputs/Templates_" + classifier_name + template_name + "_" + region + filename_suffix;
		if(classifier_name == "DNN") {input_name+= "_" + DNN_type;}
		input_name+= ".root";
	}

	if(!Check_File_Existence(input_name))
	{
		cout<<FRED("File "<<input_name<<" (<-> file containing prefit templates) not found ! Did you specify the region/background ? Abort")<<endl;
		return;
	}
	else {cout<<FBLU("--> Using file ")<<input_name<<FBLU(" instead (NB : only stat. error will be included)")<<endl;}
	cout<<endl<<endl<<endl;

	//Input file containing histos
	TFile* file_input = 0;
	file_input = TFile::Open(input_name);

	//TH1F* to retrieve distributions
	TH1F* h_tmp = 0; //Tmp storing histo

	vector<vector<TH1F*>> v2_histos(v_samples.size()); //store histos, for each sample/syst


// #       ####   ####  #####   ####
// #      #    # #    # #    # #
// #      #    # #    # #    #  ####
// #      #    # #    # #####       #
// #      #    # #    # #      #    #
// ######  ####   ####  #       ####

	//Combine output : all histos are given for subcategories --> Need to sum them all
	for(int ichan=0; ichan<channel_list.size(); ichan++)
	{
		if(channel_list[ichan] != channel) {continue;}

		for(int isample = 0; isample<v_samples.size(); isample++)
		{
			cout<<endl<<UNDL(FBLU("-- Sample : "<<v_samples[isample]<<" : "))<<endl;

			v2_histos[isample].resize(v_syst.size());

            TFile* f;
            f = file_input;

			TString samplename = v_samples[isample];

            for(int isyst=0; isyst<v_syst.size(); isyst++)
            {
				if(v_samples[isample].Contains("Fake") && !v_syst[isyst].Contains("Clos") && !v_syst[isyst].Contains("FR") && v_syst[isyst] != "") {continue;}

				// cout<<"syst "<<v_syst[isyst]<<endl;

                h_tmp = 0;

    			TString histo_name = "";
    			histo_name = classifier_name + template_name + "_" + region;
    			if(channel != "") {histo_name+= "_" + channel;}
    			histo_name+= + "__" + samplename;
                if(v_syst[isyst] != "") {histo_name+= "__" + v_syst[isyst];}

    			if(!f->GetListOfKeys()->Contains(histo_name) ) {cout<<ITAL("Histogram '"<<histo_name<<"' : not found ! Skip...")<<endl; continue;}

    			h_tmp = (TH1F*) f->Get(histo_name);
				h_tmp->SetDirectory(0); //Dis-associate from TFile
    			// cout<<"histo_name "<<histo_name<<endl;
                // cout<<"h_tmp->Integral() = "<<h_tmp->Integral()<<endl;

 //  ####   ####  #       ####  #####   ####
 // #    # #    # #      #    # #    # #
 // #      #    # #      #    # #    #  ####
 // #      #    # #      #    # #####       #
 // #    # #    # #      #    # #   #  #    #
 //  ####   ####  ######  ####  #    #  ####

    			//Use color vector filled in main()
    			// h_tmp->SetFillStyle(1001);
				h_tmp->SetFillColor(kWhite);

				h_tmp->SetLineColor(v_colors[isample]);

				//HARDCODED
				if(v_syst[isyst] == "JESUp") {h_tmp->SetLineColor(kRed);}
				else if(v_syst[isyst] == "JESDown") {h_tmp->SetLineColor(kBlue);}

				// h_tmp->SetLineColor(v_colors[isample]+isyst);
				// cout<<"v_colors[isample] "<<v_colors[isample]<<endl;

    			h_tmp->SetLineWidth(3);

				h_tmp->SetMaximum(h_tmp->GetMaximum()*1.5);
				if(normalize) {h_tmp->SetMaximum(0.5);}

                if(v_syst[isyst] != "") {h_tmp->SetLineStyle(2);}

    			if(normalize) {h_tmp->Scale(1./h_tmp->Integral() );}

                v2_histos[isample][isyst] = (TH1F*) h_tmp->Clone();

				// cout<<"v2_histos["<<isample<<"]["<<isyst<<"]->Integral() "<<v2_histos[isample][isyst]->Integral()<<endl;

    			delete h_tmp; h_tmp = 0;
            } //end syst loop

			// f->Close();
		} //end sample loop
	} //subcat loop


// #####  #####    ##   #    #
// #    # #    #  #  #  #    #
// #    # #    # #    # #    #
// #    # #####  ###### # ## #
// #    # #   #  #    # ##  ##
// #####  #    # #    # #    #

	//Canvas definition
	Load_Canvas_Style();
	gStyle->SetOptTitle(1);
	TCanvas* c = new TCanvas("", "", 1000, 800);
	c->SetTopMargin(0.1);

	// c->SetLogy();

	TLegend* qw;
	qw = new TLegend(0.75,.70,1.,1.);

	c->cd();

	for(int isample=0; isample<v2_histos.size(); isample++)
	{
		TString systlist = "";

		for(int isyst=0; isyst<v_syst.size(); isyst++)
		{
			if(v_samples[isample].Contains("Fake") && !v_syst[isyst].Contains("Clos") && !v_syst[isyst].Contains("FR") && v_syst[isyst] != "") {continue;}

			if(v2_histos[isample][isyst] == 0) {cout<<"Null histo ! Skip"<<endl; continue;}

			if(isample == 0)
			{
				if(v_syst[isyst] != "")
				{
					systlist+= " / " + v_syst[isyst];
				}

				v2_histos[isample][isyst]->SetTitle(systlist);
				// if(normalize) {v2_histos[isample][isyst]->SetMaximum(0.45);}
				// else {v2_histos[isample][isyst]->SetMaximum(6.);}
				// else {v2_histos[isample][isyst]->SetMaximum(11.);}

				if(drawInputVars) {v2_histos[isample][isyst]->GetXaxis()->SetTitle(template_name);}
				else {v2_histos[isample][isyst]->GetXaxis()->SetTitle(classifier_name+" (vs "+template_name + ")");}
			}

			if(isample < v2_histos.size() - 1)
			{
				if(v_groups[isample] == v_groups[isample+1])
				{
					v2_histos[isample+1][isyst]->Add(v2_histos[isample][isyst]); //Merge with next sample
					continue; //Will draw merged histo, not this single one
				}
			}

			if(normalize) {v2_histos[isample][isyst]->SetMaximum(0.5);}
			v2_histos[isample][isyst]->Draw("hist same");

			if(v_syst[isyst] == "")
			{
				if(v_groups[isample] == "tH") {qw->AddEntry(v2_histos[isample][isyst], "tH+ttH", "L");}
				else if(v_groups[isample].Contains("ttH")) {qw->AddEntry(v2_histos[isample][isyst], "ttH", "L");}
				else if(v_groups[isample].Contains("ttW") ) {qw->AddEntry(v2_histos[isample][isyst], "t#bar{t}W", "L");}
				else if(v_groups[isample] == "ttZ") {qw->AddEntry(v2_histos[isample][isyst], "t#bar{t}Z", "L");}
				// else if(v_groups[isample] == "WZ") {qw->AddEntry(v2_histos[isample][isyst], "EWK", "L");}
				else if(v_groups[isample] == "Fakes") {qw->AddEntry(v2_histos[isample][isyst], "Non-prompt", "L");}
				else if(v_groups[isample] == "QFlip") {qw->AddEntry(v2_histos[isample][isyst], "Flip", "L");}
				else if(v_groups[isample].Contains("GammaConv") ) {qw->AddEntry(v2_histos[isample][isyst], "#gamma-conv.", "L");}
				else {qw->AddEntry(v2_histos[isample][isyst], v_samples[isample], "L");}
			}

			//HARDCODED
			if(v_syst[isyst] == "JESUp") {qw->AddEntry(v2_histos[isample][isyst], "JES Up", "L");}
			if(v_syst[isyst] == "JESDown") {qw->AddEntry(v2_histos[isample][isyst], "JES Down", "L");}
		}
	} //sample loop

	qw->Draw("same");


//  ####   ####   ####  #    # ###### ##### #  ####   ####
// #    # #    # #      ##  ## #        #   # #    # #
// #      #    #  ####  # ## # #####    #   # #       ####
// #      #    #      # #    # #        #   # #           #
// #    # #    # #    # #    # #        #   # #    # #    #
//  ####   ####   ####  #    # ######   #   #  ####   ####

//----------------
// CAPTIONS //
//----------------
// -- using https://twiki.cern.ch/twiki/pub/CMS/Internal/FigGuidelines

	float l = c->GetLeftMargin();
	float t = c->GetTopMargin();

	TString cmsText = "CMS";
	TLatex latex;
	latex.SetNDC();
	latex.SetTextAngle(0);
	latex.SetTextColor(kBlack);

	latex.SetTextFont(61);
	latex.SetTextAlign(11);
	latex.SetTextSize(0.06);
	// latex.DrawLatex(l + 0.01, 0.92, cmsText);

	TString extraText = "Preliminary";
	latex.SetTextFont(52);
	latex.SetTextSize(0.05);
	// if(draw_preliminary_label)
	{
		// latex.DrawLatex(l + 0.12, 0.92, extraText);
	}

	float lumi = ref_luminosity * luminosity_rescale;
	TString lumi_13TeV = Convert_Number_To_TString(lumi);
	lumi_13TeV += " fb^{-1} (13 TeV)";

	latex.SetTextFont(42);
	latex.SetTextAlign(31);
	latex.SetTextSize(0.04);

	// latex.DrawLatex(0.78, 0.92,lumi_13TeV);

	//------------------
	//-- Channel info
	TLatex text2 ;
	text2.SetNDC();
	text2.SetTextAlign(13);
	text2.SetTextSize(0.045);
	text2.SetTextFont(42);

	TString info_data;
    info_data = "l^{#pm}l^{#pm}l^{#pm}";

	// if(h_sum_data->GetBinContent(h_sum_data->GetNbinsX() ) > h_sum_data->GetBinContent(1) ) {text2.DrawLatex(0.55,0.87,info_data);}
	// else {text2.DrawLatex(0.20,0.87,info_data);}
	text2.DrawLatex(0.23,0.86,info_data);



// #    # #####  # ##### ######     ####  #    # ##### #####  #    # #####
// #    # #    # #   #   #         #    # #    #   #   #    # #    #   #
// #    # #    # #   #   #####     #    # #    #   #   #    # #    #   #
// # ## # #####  #   #   #         #    # #    #   #   #####  #    #   #
// ##  ## #   #  #   #   #         #    # #    #   #   #      #    #   #
// #    # #    # #   #   ######     ####   ####    #   #       ####    #

	mkdir("plots", 0777);
	mkdir("plots/templates_shapes", 0777);

	//Output
	TString output_plot_name = "plots/templates_shapes/";
	output_plot_name+= classifier_name + template_name + "_" + region +"_templatesShapes";
	if(channel != "") {output_plot_name+= "_" + channel;}
	output_plot_name+= this->filename_suffix + this->plot_extension;

	c->SaveAs(output_plot_name);

	for(int isample=0; isample<v2_histos.size(); isample++)
	{
		for(int isyst=0; isyst<v_syst.size(); isyst++)
		{
			delete v2_histos[isample][isyst];
		}
	}

	delete c; c = NULL;
	delete qw; qw = NULL;

	return;
}