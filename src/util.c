/* This is just a really cumbersome way to register an on.exit() in C */

#define R_NO_REMAP
#define STRICT_R_HEADERS
#include <Rinternals.h>

SEXP R_new_ptr(){
  return R_MakeExternalPtr(NULL, R_NilValue, R_NilValue);
}

SEXP R_run_cleanup(SEXP ptr){
  SEXP w = R_ExternalPtrProtected(ptr);
  if(w && TYPEOF(w) == WEAKREFSXP)
    R_RunWeakRefFinalizer(w);
  return ptr;
}

void run_on_exit(SEXP ptr, R_CFinalizer_t fun, void *data){
  R_SetExternalPtrAddr(ptr, data);
  SEXP w = R_MakeWeakRefC(ptr, R_NilValue, fun, TRUE);
  R_SetExternalPtrProtected(ptr, w);
}
