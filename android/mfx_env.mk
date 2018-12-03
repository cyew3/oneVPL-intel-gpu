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

# Enable build with CPLib by special key.
# In the future it will be always enabled
ifeq ($(MFX_ENABLE_CPLIB),)
  MFX_ENABLE_CPLIB:=true
endif
