#include"CNVState.hpp"


//
// The configuration is stored in non volatile memory.  The only native NV memory
//   on the pico is the program flash, so that's where we save it.
//
class CNVFlash : public CNVState {
    typedef CNVState super;
public:
    CNVFlash();
    bool commit() override;
    void zap() override;
};

