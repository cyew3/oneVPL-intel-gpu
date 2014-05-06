################################################################################
# First Version, lack of license
# Athor: Ralph Waters
# Date: 2012-02-14
################################################################################


# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
ippcp_6_1/src/pcpbn32ca.c \
ippcp_6_1/src/pcpbnca.c \
ippcp_6_1/src/pcpbnextpxca.c \
ippcp_6_1/src/pcpbnpxca.c \
ippcp_6_1/src/pcpbnresourceca.c \
ippcp_6_1/src/pcpbnsetca.c \
ippcp_6_1/src/pcpbnuca.c \
ippcp_6_1/src/pcpbnuextpxca.c \
ippcp_6_1/src/pcpbnupxca.c \
ippcp_6_1/src/pcpbnusetca.c \
ippcp_6_1/src/pcpciphertoolpxca.c \
ippcp_6_1/src/pcpeccpdpca.c \
ippcp_6_1/src/pcpeccpgenkeyca.c \
ippcp_6_1/src/pcpeccpinitca.c \
ippcp_6_1/src/pcpeccpmethod128ca.c \
ippcp_6_1/src/pcpeccpmethod192ca.c \
ippcp_6_1/src/pcpeccpmethod224ca.c \
ippcp_6_1/src/pcpeccpmethod256ca.c \
ippcp_6_1/src/pcpeccpmethod384ca.c \
ippcp_6_1/src/pcpeccpmethod521ca.c \
ippcp_6_1/src/pcpeccpmethodcomca.c \
ippcp_6_1/src/pcpeccppointca.c \
ippcp_6_1/src/pcpeccppointopca.c \
ippcp_6_1/src/pcpeccpsetkeyca.c \
ippcp_6_1/src/pcpeccpsigndsaca.c \
ippcp_6_1/src/pcpeccpstdca.c \
ippcp_6_1/src/pcpeccpvalidatekeyca.c \
ippcp_6_1/src/pcpeccpverifydsaca.c \
ippcp_6_1/src/pcpecgfp.c \
ippcp_6_1/src/pcpecgfppoint.c \
ippcp_6_1/src/pcpecgfppointca.c \
ippcp_6_1/src/pcpecgfpx.c \
ippcp_6_1/src/pcpecgfpxpoint.c \
ippcp_6_1/src/pcpecgfpxpointca.c \
ippcp_6_1/src/pcpgfp.c \
ippcp_6_1/src/pcpgfpca.c \
ippcp_6_1/src/pcpgfpext.c \
ippcp_6_1/src/pcpgfpextca.c \
ippcp_6_1/src/pcpgfpqext.c \
ippcp_6_1/src/pcpgfpqextca.c \
ippcp_6_1/src/pcphmacsha256ca.c \
ippcp_6_1/src/pcpmontexpbinca.c \
ippcp_6_1/src/pcpmontgomeryca.c \
ippcp_6_1/src/pcpmontmulca.c \
ippcp_6_1/src/pcpmontmulpxca.c \
ippcp_6_1/src/pcpmulbnupxca.c \
ippcp_6_1/src/pcppma128.c \
ippcp_6_1/src/pcppma192.c \
ippcp_6_1/src/pcppma224.c \
ippcp_6_1/src/pcppma256.c \
ippcp_6_1/src/pcppma384.c \
ippcp_6_1/src/pcppma521.c \
ippcp_6_1/src/pcpprimeginitca.c \
ippcp_6_1/src/pcpprngenca.c \
ippcp_6_1/src/pcpprngsetca.c \
ippcp_6_1/src/pcpprnginitca.c \
ippcp_6_1/src/pcprij128decryptecbca.c \
ippcp_6_1/src/pcprij128decryptcbcca.c \
ippcp_6_1/src/pcprij128decryptpxca.c \
ippcp_6_1/src/pcprij128encryptecbca.c \
ippcp_6_1/src/pcprij128encryptcbcca.c \
ippcp_6_1/src/pcprij128encryptpxca.c \
ippcp_6_1/src/pcprij128ctrca.c \
ippcp_6_1/src/pcprij128initca.c \
ippcp_6_1/src/pcprij128safe.c \
ippcp_6_1/src/pcprij128safedecpxca.c \
ippcp_6_1/src/pcprij128safeencpxca.c \
ippcp_6_1/src/pcprijdecsboxca.c \
ippcp_6_1/src/pcprijdectableca.c \
ippcp_6_1/src/pcprijencsboxca.c \
ippcp_6_1/src/pcprijenctableca.c \
ippcp_6_1/src/pcprijkeysca.c \
ippcp_6_1/src/pcpsha1cntca.c \
ippcp_6_1/src/pcpsha1pxca.c \
ippcp_6_1/src/pcpsha256ca.c \
ippcp_6_1/src/pcpsha256cntca.c \
ippcp_6_1/src/pcpsha256pxca.c \
ippcp_6_1/src/pcptatepade.c \
ippcp_6_1/src/pcptatepadeca.c 

OBJS += \
ippcp_6_1/src/pcpbn32ca.o \
ippcp_6_1/src/pcpbnca.o \
ippcp_6_1/src/pcpbnextpxca.o \
ippcp_6_1/src/pcpbnpxca.o \
ippcp_6_1/src/pcpbnresourceca.o \
ippcp_6_1/src/pcpbnsetca.o \
ippcp_6_1/src/pcpbnuca.o \
ippcp_6_1/src/pcpbnuextpxca.o \
ippcp_6_1/src/pcpbnupxca.o \
ippcp_6_1/src/pcpbnusetca.o \
ippcp_6_1/src/pcpciphertoolpxca.o \
ippcp_6_1/src/pcpeccpdpca.o \
ippcp_6_1/src/pcpeccpgenkeyca.o \
ippcp_6_1/src/pcpeccpinitca.o \
ippcp_6_1/src/pcpeccpmethod128ca.o \
ippcp_6_1/src/pcpeccpmethod192ca.o \
ippcp_6_1/src/pcpeccpmethod224ca.o \
ippcp_6_1/src/pcpeccpmethod256ca.o \
ippcp_6_1/src/pcpeccpmethod384ca.o \
ippcp_6_1/src/pcpeccpmethod521ca.o \
ippcp_6_1/src/pcpeccpmethodcomca.o \
ippcp_6_1/src/pcpeccppointca.o \
ippcp_6_1/src/pcpeccppointopca.o \
ippcp_6_1/src/pcpeccpsetkeyca.o \
ippcp_6_1/src/pcpeccpsigndsaca.o \
ippcp_6_1/src/pcpeccpstdca.o \
ippcp_6_1/src/pcpeccpvalidatekeyca.o \
ippcp_6_1/src/pcpeccpverifydsaca.o \
ippcp_6_1/src/pcpecgfp.o \
ippcp_6_1/src/pcpecgfppoint.o \
ippcp_6_1/src/pcpecgfppointca.o \
ippcp_6_1/src/pcpecgfpx.o \
ippcp_6_1/src/pcpecgfpxpoint.o \
ippcp_6_1/src/pcpecgfpxpointca.o \
ippcp_6_1/src/pcpgfp.o \
ippcp_6_1/src/pcpgfpca.o \
ippcp_6_1/src/pcpgfpext.o \
ippcp_6_1/src/pcpgfpextca.o \
ippcp_6_1/src/pcpgfpqext.o \
ippcp_6_1/src/pcpgfpqextca.o \
ippcp_6_1/src/pcphmacsha256ca.o \
ippcp_6_1/src/pcpmontexpbinca.o \
ippcp_6_1/src/pcpmontgomeryca.o \
ippcp_6_1/src/pcpmontmulca.o \
ippcp_6_1/src/pcpmontmulpxca.o \
ippcp_6_1/src/pcpmulbnupxca.o \
ippcp_6_1/src/pcppma128.o \
ippcp_6_1/src/pcppma192.o \
ippcp_6_1/src/pcppma224.o \
ippcp_6_1/src/pcppma256.o \
ippcp_6_1/src/pcppma384.o \
ippcp_6_1/src/pcppma521.o \
ippcp_6_1/src/pcpprimeginitca.o \
ippcp_6_1/src/pcpprngenca.o \
ippcp_6_1/src/pcpprngsetca.o \
ippcp_6_1/src/pcpprnginitca.o \
ippcp_6_1/src/pcprij128decryptecbca.o \
ippcp_6_1/src/pcprij128decryptcbcca.o \
ippcp_6_1/src/pcprij128decryptpxca.o \
ippcp_6_1/src/pcprij128encryptecbca.o \
ippcp_6_1/src/pcprij128encryptcbcca.o \
ippcp_6_1/src/pcprij128encryptpxca.o \
ippcp_6_1/src/pcprij128ctrca.o \
ippcp_6_1/src/pcprij128initca.o \
ippcp_6_1/src/pcprij128safe.o \
ippcp_6_1/src/pcprij128safedecpxca.o \
ippcp_6_1/src/pcprij128safeencpxca.o \
ippcp_6_1/src/pcprijdecsboxca.o \
ippcp_6_1/src/pcprijdectableca.o \
ippcp_6_1/src/pcprijencsboxca.o \
ippcp_6_1/src/pcprijenctableca.o \
ippcp_6_1/src/pcprijkeysca.o \
ippcp_6_1/src/pcpsha1cntca.o \
ippcp_6_1/src/pcpsha1pxca.o \
ippcp_6_1/src/pcpsha256ca.o \
ippcp_6_1/src/pcpsha256cntca.o \
ippcp_6_1/src/pcpsha256pxca.o \
ippcp_6_1/src/pcptatepade.o \
ippcp_6_1/src/pcptatepadeca.o 

C_DEPS += \
ippcp_6_1/src/pcpbn32ca.d \
ippcp_6_1/src/pcpbnca.d \
ippcp_6_1/src/pcpbnextpxca.d \
ippcp_6_1/src/pcpbnpxca.d \
ippcp_6_1/src/pcpbnresourceca.d \
ippcp_6_1/src/pcpbnsetca.d \
ippcp_6_1/src/pcpbnuca.d \
ippcp_6_1/src/pcpbnuextpxca.d \
ippcp_6_1/src/pcpbnupxca.d \
ippcp_6_1/src/pcpbnusetca.d \
ippcp_6_1/src/pcpciphertoolpxca.d \
ippcp_6_1/src/pcpeccpdpca.d \
ippcp_6_1/src/pcpeccpgenkeyca.d \
ippcp_6_1/src/pcpeccpinitca.d \
ippcp_6_1/src/pcpeccpmethod128ca.d \
ippcp_6_1/src/pcpeccpmethod192ca.d \
ippcp_6_1/src/pcpeccpmethod224ca.d \
ippcp_6_1/src/pcpeccpmethod256ca.d \
ippcp_6_1/src/pcpeccpmethod384ca.d \
ippcp_6_1/src/pcpeccpmethod521ca.d \
ippcp_6_1/src/pcpeccpmethodcomca.d \
ippcp_6_1/src/pcpeccppointca.d \
ippcp_6_1/src/pcpeccppointopca.d \
ippcp_6_1/src/pcpeccpsetkeyca.d \
ippcp_6_1/src/pcpeccpsigndsaca.d \
ippcp_6_1/src/pcpeccpstdca.d \
ippcp_6_1/src/pcpeccpvalidatekeyca.d \
ippcp_6_1/src/pcpeccpverifydsaca.d \
ippcp_6_1/src/pcpecgfp.d \
ippcp_6_1/src/pcpecgfppoint.d \
ippcp_6_1/src/pcpecgfppointca.d \
ippcp_6_1/src/pcpecgfpx.d \
ippcp_6_1/src/pcpecgfpxpoint.d \
ippcp_6_1/src/pcpecgfpxpointca.d \
ippcp_6_1/src/pcpgfp.d \
ippcp_6_1/src/pcpgfpca.d \
ippcp_6_1/src/pcpgfpext.d \
ippcp_6_1/src/pcpgfpextca.d \
ippcp_6_1/src/pcpgfpqext.d \
ippcp_6_1/src/pcpgfpqextca.d \
ippcp_6_1/src/pcphmacsha256ca.d \
ippcp_6_1/src/pcpmontexpbinca.d \
ippcp_6_1/src/pcpmontgomeryca.d \
ippcp_6_1/src/pcpmontmulca.d \
ippcp_6_1/src/pcpmontmulpxca.d \
ippcp_6_1/src/pcpmulbnupxca.d \
ippcp_6_1/src/pcppma128.d \
ippcp_6_1/src/pcppma192.d \
ippcp_6_1/src/pcppma224.d \
ippcp_6_1/src/pcppma256.d \
ippcp_6_1/src/pcppma384.d \
ippcp_6_1/src/pcppma521.d \
ippcp_6_1/src/pcpprimeginitca.d \
ippcp_6_1/src/pcpprngenca.d \
ippcp_6_1/src/pcpprngsetca.d \
ippcp_6_1/src/pcpprnginitca.d \
ippcp_6_1/src/pcprij128decryptecbca.d \
ippcp_6_1/src/pcprij128decryptcbcca.d \
ippcp_6_1/src/pcprij128decryptpxca.d \
ippcp_6_1/src/pcprij128encryptecbca.d \
ippcp_6_1/src/pcprij128encryptcbcca.d \
ippcp_6_1/src/pcprij128encryptpxca.d \
ippcp_6_1/src/pcprij128ctrca.d \
ippcp_6_1/src/pcprij128initca.d \
ippcp_6_1/src/pcprij128safe.d \
ippcp_6_1/src/pcprij128safedecpxca.d \
ippcp_6_1/src/pcprij128safeencpxca.d \
ippcp_6_1/src/pcprijdecsboxca.d \
ippcp_6_1/src/pcprijdectableca.d \
ippcp_6_1/src/pcprijencsboxca.d \
ippcp_6_1/src/pcprijenctableca.d \
ippcp_6_1/src/pcprijkeysca.d \
ippcp_6_1/src/pcpsha1cntca.d \
ippcp_6_1/src/pcpsha1pxca.d \
ippcp_6_1/src/pcpsha256ca.d \
ippcp_6_1/src/pcpsha256cntca.d \
ippcp_6_1/src/pcpsha256pxca.d \
ippcp_6_1/src/pcptatepade.d \
ippcp_6_1/src/pcptatepadeca.d 


# Each subdirectory must supply rules for building sources it contributes
ippcp_6_1/src/%.o: ippcp_6_1/src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler IN ippcp_6_1'
	gcc -O0 -g3 -Wall -D_IPP_v50_ -c -fPIC $(C_FLAGS) $(C_INCL) -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


