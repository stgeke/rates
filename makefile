#########################################################
# USER SETTINGS
#########################################################
export CXX = mpic++

export OCCA_CUDA_ENABLED=1
export OCCA_HIP_ENABLED=0
export OCCA_OPENCL_ENABLED=0
export OCCA_METAL_ENABLED=0

#########################################################


include ${OCCA_DIR}/scripts/Makefile

.PHONY: all rates clean 

all: rates
	echo "please set the following env-vars:"; \
	echo "  export OCCA_DIR=${PREFIX}/occa"; \
	echo "  export LD_LIBRARY_PATH=\$$LD_LIBRARY_PATH:\$$OCCA_DIR/lib"; \
	echo ""; \
	echo "compilation successful!"; \
	echo "";

%.o: %.cpp ; $(CXX) -O2 -march=native -ffast-math  -o $@ -c $< 

rates: gri.o
	$(CXX) $(compilerFlags) $(flags) -I$(OCCA_DIR)/include -o rates gri.o rates.cpp -L $(OCCA_DIR)/lib $(linkerFlags) 

clean:
	rm -rf *.o rates
