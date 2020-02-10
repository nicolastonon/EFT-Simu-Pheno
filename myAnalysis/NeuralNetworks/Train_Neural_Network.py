# Nicolas TONON (DESY)
# Train fully-connected neural networks with Keras (tf back-end)
# //--------------------------------------------

'''
#TODO#
- argparse (model, ...)

#NOTES#
- fit() is for training the model with the given inputs (and corresponding training labels).
- evaluate() is for evaluating the already trained model using the validation (or test) data and the corresponding labels. Returns the loss value and metrics values for the model.
- predict() is for the actual prediction. It generates output predictions for the input samples.

- Using abs event weights; if using relative, may have problems when computing class weights
'''


# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------
 #######  ########  ######## ####  #######  ##    ##  ######
##     ## ##     ##    ##     ##  ##     ## ###   ## ##    ##
##     ## ##     ##    ##     ##  ##     ## ####  ## ##
##     ## ########     ##     ##  ##     ## ## ## ##  ######
##     ## ##           ##     ##  ##     ## ##  ####       ##
##     ## ##           ##     ##  ##     ## ##   ### ##    ##
 #######  ##           ##    ####  #######  ##    ##  ######
# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------

# Analysis options
# //--------------------------------------------
# -- Choose here what data you want to consider (separate ntuples per year) ; same convention as for main analysis code
# Naming convention enforced : 2016+2017 <-> "201617" ; etc.; 2016+2017+2018 <-> "Run2" # NB : years must be placed in the right order !
_lumi_years = []
_lumi_years.append("2016")
_lumi_years.append("2017")
_lumi_years.append("2018")

#Signal process must be first
_processClasses_list = [["tZq"],
                # ["ttZ"]]
                ["ttZ"], ["ttW", "ttH", "WZ", "ZZ4l", "DY", "TTbar_DiLep"]]
                # ["ttZ", "ttW", "ttH", "WZ", "ZZ4l", "DY", "TTbar_DiLep",]]

_labels_list =  ["tZq",
                "ttZ", "Backgrounds"]
                # "Backgrounds"]

cuts = "passedBJets==1" #Event selection, both for train/test ; "1" <-> no cut
# //--------------------------------------------

#--- Training options
# //--------------------------------------------
_nepochs = 50 #Number of training epochs (<-> nof times the full training dataset is shown to the NN)
_batchSize = 256 #Batch size (<-> nof events fed to the network before its parameter get updated)
# _nof_output_nodes = 3 #1 (binary) or N (multiclass)

_maxEvents_perClass = -1 #max nof events to be used for each process ; -1 <-> all events
_nEventsTot_train = -1; _nEventsTot_test = -1  #nof events to be used for training & testing ; -1 <-> use _maxEvents_perClass & _splitTrainEventFrac params instead
_splitTrainEventFrac = 0.8 #Fraction of events to be used for training (1 <-> use all requested events for training)
# //--------------------------------------------

# Define list of input variables
# //--------------------------------------------
var_list = []
var_list.append("maxDijetDelR")
var_list.append("dEtaFwdJetBJet")
var_list.append("dEtaFwdJetClosestLep")
var_list.append("mHT")
var_list.append("mTW")
var_list.append("Mass_3l")
var_list.append("forwardJetAbsEta")
var_list.append("jPrimeAbsEta")
var_list.append("maxDeepCSV")
var_list.append("delRljPrime")
var_list.append("lAsymmetry")
var_list.append("maxDijetMass")
var_list.append("maxDelPhiLL")









# //--------------------------------------------
#Filtering out manually some unimportant warnings
# import warnings
# warnings.filterwarnings("ignore", message="tensorflow:sample_weight modes were coerced")

# --------------------------------------------
# Standard python import
import os    # mkdir
import sys    # exit
import time   # time accounting
import getopt # command line parser
# //--------------------------------------------
import tensorflow
import keras
import numpy as np
from sklearn.metrics import roc_curve, auc, roc_auc_score, accuracy_score
from tensorflow.keras.models import load_model

from Utils.FreezeSession import freeze_session
from Utils.Helper import batchOutput, Write_Variables_To_TextFile, TimeHistory, Get_LumiName, SanityChecks_Parameters
from Utils.Model import Create_Model
from Utils.Callbacks import Get_Callbacks
from Utils.GetData import Get_Data_For_DNN_Training
from Utils.Optimizer import Get_Loss_Optim_Metrics
from Utils.ColoredPrintout import colors
from Utils.Output_Plots_Histos import Create_TrainTest_ROC_Histos, Create_Control_Plots
# //--------------------------------------------
# //--------------------------------------------















# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------
######## ########     ###    #### ##    ##
   ##    ##     ##   ## ##    ##  ###   ##
   ##    ##     ##  ##   ##   ##  ####  ##
   ##    ########  ##     ##  ##  ## ## ##
   ##    ##   ##   #########  ##  ##  ####
   ##    ##    ##  ##     ##  ##  ##   ###
   ##    ##     ## ##     ## #### ##    ##

######## ########  ######  ########
   ##    ##       ##    ##    ##
   ##    ##       ##          ##
   ##    ######    ######     ##
   ##    ##             ##    ##
   ##    ##       ##    ##    ##
   ##    ########  ######     ##

######## ##     ##    ###    ##
##       ##     ##   ## ##   ##
##       ##     ##  ##   ##  ##
######   ##     ## ##     ## ##
##        ##   ##  ######### ##
##         ## ##   ##     ## ##
########    ###    ##     ## ########
# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------

def Train_Test_Eval_PureKeras(_lumi_years, _processClasses_list, _labels_list, var_list, cuts, _nepochs, _batchSize, _maxEvents_perClass, _splitTrainEventFrac, _nEventsTot_train, _nEventsTot_test):

    SanityChecks_Parameters(_processClasses_list, _labels_list)

    #Read luminosity choice
    lumiName = Get_LumiName(_lumi_years)

    # Main paths
    weight_dir = "../weights/DNN/" + lumiName + '/'
    os.makedirs(weight_dir, exist_ok=True)

    #Top directory containing all input ntuples
    _ntuples_dir = "../input_ntuples/"

    #Determine/store number of process classes
    _nof_output_nodes = len(_processClasses_list) #1 output node per class
    if _nof_output_nodes == 2: #Special case : 2 classes -> binary classification -> 1 output node only
        _nof_output_nodes = 1

    print('\n\n')
    print(colors.bg.orange, colors.bold, "=====================================", colors.reset)
    print('\t', colors.fg.orange, colors.bold, "DNN Training", colors.reset)
    print(colors.bg.orange, colors.bold, "=====================================", colors.reset, '\n\n')

    #Get data
    print(colors.fg.lightblue, "--- Read and shape the data...", colors.reset); print('\n')
    x_train, y_train, x_test, y_test, PhysicalWeights_train, PhysicalWeights_test, LearningWeights_train, LearningWeights_test, x, y, PhysicalWeights_allClasses, LearningWeights_allClasses = Get_Data_For_DNN_Training(weight_dir, _lumi_years, _ntuples_dir, _processClasses_list, _labels_list, var_list, cuts, _nof_output_nodes, _maxEvents_perClass, _splitTrainEventFrac, _nEventsTot_train, _nEventsTot_test, lumiName)

    #Get model, compile
    print('\n'); print(colors.fg.lightblue, "--- Create the Keras model...", colors.reset); print('\n')
    model = Create_Model(weight_dir, "DNN", _nof_output_nodes, var_list) #-- add default args

    print('\n'); print(colors.fg.lightblue, "--- Define the loss function & metrics...", colors.reset); print('\n')
    _loss, _optim, _metrics = Get_Loss_Optim_Metrics(_nof_output_nodes)

    print('\n'); print(colors.fg.lightblue, "--- Compile the Keras model...", colors.reset); print('\n')
    model.compile(loss=_loss, optimizer=_optim, metrics=[_metrics]) #For multiclass classification

    callbacks_list = Get_Callbacks(weight_dir)

    #Fit model #Slow for full Run 2 ! Should find a way to feed the data in batches, to avoid loading all in memory ?
    print('\n'); print(colors.fg.lightblue, "--- Train (fit) DNN on training sample...", colors.reset, " (may take a while)"); print('\n')
    history = model.fit(x_train, y_train, validation_data=(x_test, y_test), epochs=_nepochs, batch_size=_batchSize, sample_weight=LearningWeights_train, callbacks=callbacks_list, shuffle=True, verbose=1)

    history.history['lr'] # returns a list of learning rates over the epochs

    #-- Can access weights and biases of any layer
    # weights_layer, biases_layer = model.layers[0].get_weights(); print(weights_layer.shape); print(biases_layer.shape); print(weights_layer); print(biases_layer[0:2])

    # Evaluate the model (metrics)
    print('\n'); print(colors.fg.lightblue, "--- Evaluate DNN performance on test sample...", colors.reset); print('\n')
    score = model.evaluate(x_test, y_test, batch_size=_batchSize, sample_weight=PhysicalWeights_test)
    # print(score)


  ####    ##   #    # ######    #    #  ####  #####  ###### #
 #       #  #  #    # #         ##  ## #    # #    # #      #
  ####  #    # #    # #####     # ## # #    # #    # #####  #
      # ###### #    # #         #    # #    # #    # #      #
 #    # #    #  #  #  #         #    # #    # #    # #      #
  ####  #    #   ##   ######    #    #  ####  #####  ###### ######

    print('\n'); print(colors.fg.lightblue, "--- Save model...", colors.reset);

    #Serialize model to HDF5
    h5model_outname = weight_dir + 'model.h5'
    model.save(h5model_outname)

    # Save the model architecture
    with open(weight_dir + 'arch_DNN.json', 'w') as json_file:
        json_file.write(model.to_json())

    #Save list of variables #Done in data transformation function now
    # Write_Variables_To_TextFile(weight_dir, var_list)


 ###### #####  ###### ###### ###### ######     ####  #####    ##   #####  #    #
 #      #    # #      #          #  #         #    # #    #  #  #  #    # #    #
 #####  #    # #####  #####     #   #####     #      #    # #    # #    # ######
 #      #####  #      #        #    #         #  ### #####  ###### #####  #    #
 #      #   #  #      #       #     #         #    # #   #  #    # #      #    #
 #      #    # ###### ###### ###### ######     ####  #    # #    # #      #    #

# --- convert model to estimator and save model as frozen graph for c++

    print('\n'); print(colors.fg.lightblue, "--- Freeze graph...", colors.reset); print('\n')

    # tensorflow.compat.v1.keras.backend.clear_session() #Closing the last session avoids that node names get a suffix appened when opening a new session #Does not work?
    # sess = tensorflow.compat.v1.keras.backend.get_session()
    # graph = sess.graph
    with tensorflow.compat.v1.Session() as sess: #Must first open a new session #Can't manage to run code below without this... (why?)

        tensorflow.keras.backend.set_learning_phase(0) # This line must be executed before loading Keras model (why?)
        model = load_model(h5model_outname) # model has to be re-loaded

        inputs_names = [input.op.name for input in model.inputs]
        outputs_names = [output.op.name for output in model.outputs]
        # print('\ninputs: ', model.inputs)
        print('\n')
        print(colors.fg.lightgrey, '--> inputs_names: ', inputs_names[0], colors.reset, '\n')
        # print('\noutputs: ', model.outputs)
        print(colors.fg.lightgrey, '--> outputs_names: ', outputs_names[0], colors.reset, '\n')
        # tf_node_list = [n.name for n in  tensorflow.compat.v1.get_default_graph().as_graph_def().node]; print('nodes list : ', tf_node_list)
        frozen_graph = freeze_session(sess, output_names=[output.op.name for output in model.outputs])
        tensorflow.io.write_graph(frozen_graph, weight_dir, 'model.pbtxt', as_text=True)
        tensorflow.io.write_graph(frozen_graph, weight_dir, 'model.pb', as_text=False)
        print('\n'); print(colors.fg.lightgrey, '===> Successfully froze graph...', colors.reset, '\n')

        #Also append the names of the input/output nodes in the file "DNN_info.txt" containing input features names, etc. (for later use in C++ code)
        text_file = open(weight_dir + "DNN_infos.txt", "a") #Append mode
        text_file.write(inputs_names[0]); text_file.write(' -1 -1 \n'); #use end values as flags to signal these lines
        text_file.write(outputs_names[0]); text_file.write(' -2 -2 \n');
        text_file.write(_nof_output_nodes); text_file.write(' -3 -3\n');
        text_file.close()


 #####  ######  ####  #    # #      #####  ####
 #    # #      #      #    # #        #   #
 #    # #####   ####  #    # #        #    ####
 #####  #           # #    # #        #        #
 #   #  #      #    # #    # #        #   #    #
 #    # ######  ####   ####  ######   #    ####

        print('\n\n')
        print(colors.bg.orange, colors.bold, "##############################################", colors.reset)
        print(colors.fg.orange, '\t Results & Control Plots', colors.reset)
        print(colors.bg.orange, colors.bold, "##############################################", colors.reset, '\n')

        if _nof_output_nodes == 1 :
            loss = score[0]
            accuracy = score[1]
            print(colors.fg.lightgrey, '** Accuracy :', str(accuracy), colors.reset)
            print(colors.fg.lightgrey, '** Loss', str(loss), colors.reset)

        # if len(np.unique(y_train)) > 1: # prevent bug in roc_auc_score, need >=2 unique values (at least sig+bkg classes)
        #     auc_score = roc_auc_score(y_test, model.predict(x_test))
        #     auc_score_train = roc_auc_score(y_train, model.predict(x_train))
        #     print('\n'); print(colors.fg.lightgrey, '**** AUC scores ****', colors.reset)
        #     print(colors.fg.lightgrey, "-- TEST SAMPLE  \t==> " + str(auc_score), colors.reset)
        #     print(colors.fg.lightgrey, "-- TRAIN SAMPLE \t==> " + str(auc_score_train), colors.reset); print('\n')

        list_predictions_train_allNodes_allClasses, list_predictions_test_allNodes_allClasses, list_PhysicalWeightsTrain_allClasses, list_PhysicalWeightsTest_allClasses = Apply_Model_toTrainTestData(_nof_output_nodes, _processClasses_list, _labels_list, x_train, y_train, x_test, y_test, PhysicalWeights_train, PhysicalWeights_test, h5model_outname)

        Create_TrainTest_ROC_Histos(lumiName, _nof_output_nodes, _labels_list, list_predictions_train_allNodes_allClasses, list_predictions_test_allNodes_allClasses, list_PhysicalWeightsTrain_allClasses, list_PhysicalWeightsTest_allClasses, _metrics)

        Create_Control_Plots(_nof_output_nodes, _labels_list, list_predictions_train_allNodes_allClasses, list_predictions_test_allNodes_allClasses, list_PhysicalWeightsTrain_allClasses, list_PhysicalWeightsTest_allClasses, x_train, y_train, y_test, x_test, model, history, _metrics, _nof_output_nodes, weight_dir)

    #End [with ... as sess]
# //--------------------------------------------
# //--------------------------------------------















# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------
   ###    ########  ########  ##       ##    ##
  ## ##   ##     ## ##     ## ##        ##  ##
 ##   ##  ##     ## ##     ## ##         ####
##     ## ########  ########  ##          ##
######### ##        ##        ##          ##
##     ## ##        ##        ##          ##
##     ## ##        ##        ########    ##

##     ##  #######  ########  ######## ##
###   ### ##     ## ##     ## ##       ##
#### #### ##     ## ##     ## ##       ##
## ### ## ##     ## ##     ## ######   ##
##     ## ##     ## ##     ## ##       ##
##     ## ##     ## ##     ## ##       ##
##     ##  #######  ########  ######## ########
# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------

def Apply_Model_toTrainTestData(nof_output_nodes, processClasses_list, labels_list, x_train, y_train, x_test, y_test, PhysicalWeights_train, PhysicalWeights_test, savedModelName):

    print(colors.fg.lightblue, "--- Apply model to train & test data...", colors.reset); print('\n')

    # print('x_test:\n', x_test[:10]); print('y_test:\n', y_test[:10]); print('x_train:\n', x_train[:10]); print('y_train:\n', y_train[:10])

    list_xTrain_allClasses = []
    list_xTest_allClasses = []
    list_yTrain_allClasses = []
    list_yTest_allClasses = []
    list_PhysicalWeightsTrain_allClasses = []
    list_PhysicalWeightsTest_allClasses = []

    if nof_output_nodes == 1: #Binary
        list_xTrain_allClasses.append(x_train[y_train==1]); list_yTrain_allClasses.append(y_train[y_train==1]); list_PhysicalWeightsTrain_allClasses.append(PhysicalWeights_train[y_train==1])
        list_xTrain_allClasses.append(x_train[y_train==0]); list_yTrain_allClasses.append(y_train[y_train==0]); list_PhysicalWeightsTrain_allClasses.append(PhysicalWeights_train[y_train==0])
        list_xTest_allClasses.append(x_test[y_test==1]); list_yTest_allClasses.append(y_test[y_test==1]); list_PhysicalWeightsTest_allClasses.append(PhysicalWeights_test[y_test==1])
        list_xTest_allClasses.append(x_test[y_test==0]); list_yTest_allClasses.append(y_test[y_test==0]); list_PhysicalWeightsTest_allClasses.append(PhysicalWeights_test[y_test==0])
    else: #Multiclass
        for i in range(len(_processClasses_list)):
            list_xTrain_allClasses.append(x_train[y_train[:,i]==1]); list_yTrain_allClasses.append(y_train[:,i]==1); list_PhysicalWeightsTrain_allClasses.append(PhysicalWeights_train[y_train[:,i]==1])
            list_xTest_allClasses.append(x_test[y_test[:,i]==1]); list_yTest_allClasses.append(y_test[:,i]==1); list_PhysicalWeightsTest_allClasses.append(PhysicalWeights_test[y_test[:,i]==1])


 #####  #####  ###### #####  #  ####  ##### #  ####  #    #  ####
 #    # #    # #      #    # # #    #   #   # #    # ##   # #
 #    # #    # #####  #    # # #        #   # #    # # #  #  ####
 #####  #####  #      #    # # #        #   # #    # #  # #      #
 #      #   #  #      #    # # #    #   #   # #    # #   ## #    #
 #      #    # ###### #####  #  ####    #   #  ####  #    #  ####

    #--- Load model
    model = load_model(savedModelName)


    #Application (can also use : predict_classes, predict_proba)
    list_predictions_train_allNodes_allClasses = []
    list_predictions_test_allNodes_allClasses = []
    for inode in range(nof_output_nodes):

        list_predictions_train_allClasses = []
        list_predictions_test_allClasses = []
        for iclass in range(len(processClasses_list)):
            list_predictions_train_allClasses.append(model.predict(list_xTrain_allClasses[iclass])[:,inode])
            list_predictions_test_allClasses.append(model.predict(list_xTest_allClasses[iclass])[:,inode])

        list_predictions_train_allNodes_allClasses.append(list_predictions_train_allClasses)
        list_predictions_test_allNodes_allClasses.append(list_predictions_test_allClasses)


    # -- Printout of some results
    # np.set_printoptions(threshold=5) #If activated, will print full numpy arrays
    # print("-------------- FEW EXAMPLES... --------------")
    # for i in range(10):
    #     if nof_output_nodes == 1:
    #         if y_test[i]==1:
    #             true_label = "signal"
    #         else:
    #             true_label = "background"
    #         print("===> Prediction for %s event : %s" % (true_label, (list_predictions_test_allClasses[0])[i]))
    #
    #     else:
    #         for j in range(len(processClasses_list)):
    #             if y_test[i][j]==1:
    #                 true_label = labels_list[j]
    #     print("===> Outputs nodes predictions for %s event : %s" % (true_label, (list_predictions_test_allClasses[j])[i]) )
    # print("--------------\n")

    # return list_predictions_train_allClasses, list_predictions_test_allClasses, list_PhysicalWeightsTrain_allClasses, list_PhysicalWeightsTest_allClasses
    return list_predictions_train_allNodes_allClasses, list_predictions_test_allNodes_allClasses, list_PhysicalWeightsTrain_allClasses, list_PhysicalWeightsTest_allClasses

















# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------
######## ##     ## ##    ##  ######  ######## ####  #######  ##    ##
##       ##     ## ###   ## ##    ##    ##     ##  ##     ## ###   ##
##       ##     ## ####  ## ##          ##     ##  ##     ## ####  ##
######   ##     ## ## ## ## ##          ##     ##  ##     ## ## ## ##
##       ##     ## ##  #### ##          ##     ##  ##     ## ##  ####
##       ##     ## ##   ### ##    ##    ##     ##  ##     ## ##   ###
##        #######  ##    ##  ######     ##    ####  #######  ##    ##


 ######     ###    ##       ##        ######
##    ##   ## ##   ##       ##       ##    ##
##        ##   ##  ##       ##       ##
##       ##     ## ##       ##        ######
##       ######### ##       ##             ##
##    ## ##     ## ##       ##       ##    ##
 ######  ##     ## ######## ########  ######
# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------
# //--------------------------------------------

# if len(sys.argv) == 2:
    # nLep = "3l" if sys.argv[1] == True else False

#----------  Manual call to DNN training function
Train_Test_Eval_PureKeras(_lumi_years, _processClasses_list, _labels_list, var_list, cuts, _nepochs, _batchSize, _maxEvents_perClass, _splitTrainEventFrac, _nEventsTot_train, _nEventsTot_test)
