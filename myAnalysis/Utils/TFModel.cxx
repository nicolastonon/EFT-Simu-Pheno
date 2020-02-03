#include "TFModel.h"

// #include "PhysicsTools/TensorFlow/interface/TensorFlow.h" //CMSSW


TFModel::TFModel(const std::string &model_name, const unsigned _n_inputs,
    const std::string &_input_name, const unsigned _n_outputs,
    const std::string &_output_name):
    n_inputs(_n_inputs),
    n_outputs(_n_outputs),
    input_name(_input_name),
    output_name(_output_name)
{
    std::cout<<"Load tensorflow graph from "<<model_name<<std::endl<< std::endl;
    graphDef = tensorflow::loadGraphDef(model_name);

    std::cout<<"Create tensorflow session"<<std::endl<<std::endl;
    session = tensorflow::createSession((tensorflow::GraphDef*) graphDef); //1 thread by default
}

// std::vector<float> TFModel::evaluate(const double inputs[])
std::vector<float> TFModel::evaluate(float inputs[])
{
    tensorflow::Tensor input(tensorflow::DT_FLOAT, { 1, n_inputs });
    float* d = input.flat<float>().data();
    for(unsigned i=0; i < n_inputs; i++){
        *d++ = (float)inputs[i];
    }
    std::vector<tensorflow::Tensor> outputs;
    tensorflow::run((tensorflow::Session*)session, { { input_name, input } }, { output_name }, &outputs);

    std::vector<float> out;
    for(unsigned i=0; i < n_outputs; i++){
          out.push_back(outputs[0].matrix<float>()(0,i));
    }
    return out;
}

TFModel::~TFModel()
{
    std::cout << "~TFModel()"<<std::endl;

    if(session != nullptr){
        std::cout << "Close tensorflow session"<<std::endl;
        tensorflow::Session* s = (tensorflow::Session*)session;
        tensorflow::closeSession(s);
    }

    if(graphDef != nullptr){
        std::cout << "Delete tensorflow graph"<<std::endl;
        delete (tensorflow::GraphDef*)graphDef;
    }
}
