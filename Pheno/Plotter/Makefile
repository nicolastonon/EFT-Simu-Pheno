# Makefile for BDT codes

# $< = name of first dependancy
# $@ = name of target ("main")
# -c ask the compiler not to link
# -o specify target
#

#Define variables
CC=g++ -fPIC #Compiler used
DEBUG = -g
CFLAGS = -c $(DEBUG) #Compilation options
CFLAGS += -std=c++14 #Only necessary to use frugally-deep add-on (to read NN trained with Keras in C++) -- Else, can disactivate
ROOTFLAGS = `root-config --glibs --cflags`

# LFLAGS = $(DEBUG) -L/home/ntonon/myroot/lib -lTMVA
LFLAGS = $(DEBUG) -lTMVA
# LFLAGS+= -L$(PWD) #On lxplus, make sure local dir. is included in LD_LIBRARY_PATH

INCFLAGS = -I.

SRCS = $(wildcard *.cxx) #Source files are all files with .cxx extension
HDRS = $(wildcard *.h) #Header files are all files with .h extension
OBJS = $(SRCS:.cxx=.o) #Object files are all files with .o extension, which have same names as source files

DICT = myDict.cxx
LIB = myLib.so

GENPLOTTER = GenPlotter.exe
XSECPLOTTER = PlotXsecVsOperator.exe

# //--------------------------------------------
#Instructions
# all: $(GENPLOTTER) $(XSECPLOTTER)
all: $(DICT) $(LIB) $(GENPLOTTER) $(XSECPLOTTER)

$(DICT): Utils/WCPoint.h Utils/WCFit.h Utils/TH1EFT.h
	@echo "-- Creating dictionnary $(DICT) --"
	@rootcling -f $(DICT) -rml $(LIB) -rmf myLib.rootmap -c -p Utils/WCPoint.h Utils/WCFit.h Utils/TH1EFT.h Utils/LinkDef.h

$(LIB): Utils/TH1EFT.cxx $(DICT)
	@echo "-- Creating shared library $(LIB) --"
	@$(CC) $(INCFLAGS) $(ROOTFLAGS) $(DICT) Utils/TH1EFT.cxx -shared -o $(LIB)
	@echo ""

#Obtain executables from object files
$(GENPLOTTER): GenPlotter.cc Helper.h $(LIB)
	@echo "###################################""#"
	@echo "-- Creating executable ./$(GENPLOTTER) --"
	@$(CC) GenPlotter.cc -o $@ $(ROOTFLAGS) $(LFLAGS) $(INCFLAGS) $(LIB)
	@echo "-- Done --"
	@echo "###################################""#"
	@echo ""

$(XSECPLOTTER): PlotXsecVsOperator.cc Helper.h $(LIB)
	@echo "###################################""#"
	@echo "-- Creating executable ./$(XSECPLOTTER) --"
	@$(CC) PlotXsecVsOperator.cc -o $@ $(ROOTFLAGS) $(LFLAGS) $(INCFLAGS) $(LIB)
	@echo "-- Done --"
	@echo "###################################""#"
	@echo ""

#Obtain objects from source and header files
%.o: %.cxx $(HDRS)
	@echo "-- Compiling : "$@"--"
	@$(CC) $(ROOTFLAGS) $(CFLAGS) $(LFLAGS) $(INCFLAGS) $< -o $@
	@#Example :  g++ `root-config --glibs --cflags` -fPIC -c -g Func_other.cxx -o Func_other.o


#Erase all objects and executable
clean:
	@-rm $(GENPLOTTER) $(XSECPLOTTER)
	@-rm -f *.o *.exe Utils/*.o $(LIB) $(DICT) myDict* myLib*
