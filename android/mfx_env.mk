# Defines Media SDK targets to build. Should be included
# in all Android.mk files which define build targets prior
# to any other directives.

# Build HW Media SDK library
ifeq ($(MFX_IMPL_HW),)
  MFX_IMPL_HW:=true
endif

# Build SW Media SDK library
ifeq ($(MFX_IMPL_SW),)
  MFX_IMPL_SW:=true
endif
