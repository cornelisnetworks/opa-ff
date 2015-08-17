# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015, Intel Corporation
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
# 
#     * Redistributions of source code must retain the above copyright notice,
#       this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Intel Corporation nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# 
# END_ICS_COPYRIGHT8   ****************************************
# Makefile for OPENIB_HOST Project

# Include Make Control Settings
include $(TL_DIR)/$(PROJ_FILE_DIR)/Makesettings.project

# default target for project level makefile is stage
stage::
#prepfiles:: prepfiles_global
clobber:: clobber_global clobber_tools
ALL:: tools

BSP_SPECIFIC_DIRS=

#=============================================================================#
# Definitions:
#-----------------------------------------------------------------------------#

# Name of SubProjects
DS_SUBPROJECTS	= 
# name of executable or downloadable image
EXECUTABLE		= # OPENIB_FF$(EXE_SUFFIX)
# list of sub directories to build
DIRS			= \
				$(TL_DIR)/Makerules \
				$(shell $(OPTIONALDIR) $(TL_DIR)/IbAccess) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/oib_utils) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/opa_fe_utils) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Xml) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/IbaTools) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/MpiApps) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/ShmemApps) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Iih) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/TestTools) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/SrpTools) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/InicTools) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Tests) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/IbaTests) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/IbPrint) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Topology) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/opasadb) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Md5) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Lsf) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Moab) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/Dsap) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/HSM_Lib/log_library) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/HSM_Lib/ibaccess_lib) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/HSM_Lib/cs_library) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/HSM_Lib/ib_library) \
				$(shell $(OPTIONALDIR) $(TL_DIR)/fm_config) \
				$(BUILD_TARGET_OS) 
				
				# DISABLED - MWHEINZ
				#$(shell $(OPTIONALDIR) $(TL_DIR)/L8sim)


# C files (.c)
CFILES			= \
				# Add more c files here
# C++ files (.cpp)
CCFILES			= \
				# Add more cpp files here
# lex files (.lex)
LFILES			= \
				# Add more lex files here
# archive library files (basename, $ARFILES will add MOD_LIB_DIR/prefix and suffix)
LIBFILES = 
# Windows Resource Files (.rc)
RSCFILES		=
# Windows IDL File (.idl)
IDLFILE			=
# Windows Linker Module Definitions (.def) file for dll's
DEFFILE			=
# targets to build during INCLUDES phase (add public includes here)
INCLUDE_TARGETS	= \
				# Add more h hpp files here
# Non-compiled files
MISC_FILES		= 
# all source files
SOURCES			= $(CFILES) $(CCFILES) $(LFILES) $(RSCFILES) $(IDLFILE)
# Source files to include in DSP File
DSP_SOURCES		= $(INCLUDE_TARGETS) $(SOURCES) $(MISC_FILES) \
				  $(RSCFILES) $(DEFFILE) $(MAKEFILE) Makerules.project 
# all object files
OBJECTS			= $(CFILES:.c=$(OBJ_SUFFIX)) $(CCFILES:.cpp=$(OBJ_SUFFIX)) \
				  $(LFILES:.lex=$(OBJ_SUFFIX))
RSCOBJECTS		= $(RSCFILES:.rc=$(RES_SUFFIX))
# targets to build during LIBS phase
LIB_TARGETS_IMPLIB	=
LIB_TARGETS_ARLIB	= 
LIB_TARGETS_EXP		= $(LIB_TARGETS_IMPLIB:$(ARLIB_SUFFIX)=$(EXP_SUFFIX))
LIB_TARGETS_MISC	= 
# targets to build during CMDS phase
CMD_TARGETS_SHLIB	= 
CMD_TARGETS_EXE		= $(EXECUTABLE)
CMD_TARGETS_MISC	=
# files to remove during clean phase
CLEAN_TARGETS_MISC	=  
CLEAN_TARGETS		= $(OBJECTS) $(RSCOBJECTS) $(IDL_TARGETS) $(CLEAN_TARGETS_MISC)
# other files to remove during clobber phase
CLOBBER_TARGETS_MISC=
# sub-directory to install to within bin
BIN_SUBDIR		= 
# sub-directory to install to within include
INCLUDE_SUBDIR		=

# Additional Settings
#CLOCALDEBUG	= User defined C debugging compilation flags [Empty]
#CCLOCALDEBUG	= User defined C++ debugging compilation flags [Empty]
#CLOCAL	= User defined C flags for compiling [Empty]
#CCLOCAL	= User defined C++ flags for compiling [Empty]
#BSCLOCAL	= User flags for Browse File Builder [Empty]
#DEPENDLOCAL	= user defined makedepend flags [Empty]
#LINTLOCAL	= User defined lint flags [Empty]
#LOCAL_INCLUDE_DIRS	= User include directories to search for C/C++ headers [Empty]
#LDLOCAL	= User defined C flags for linking [Empty]
#IMPLIBLOCAL	= User flags for Object Lirary Manager [Empty]
#MIDLLOCAL	= User flags for IDL compiler [Empty]
#RSCLOCAL	= User flags for resource compiler [Empty]
#LOCALDEPLIBS	= User libraries to include in dependencies [Empty]
#LOCALLIBS		= User libraries to use when linking [Empty]
#				(in addition to LOCALDEPLIBS)
#LOCAL_LIB_DIRS	= User library directories for libpaths [Empty]

LOCALDEPLIBS = 

# Include Make Rules definitions and rules
include $(TL_DIR)/$(PROJ_FILE_DIR)/Makerules.project

#=============================================================================#
# Overrides:
#-----------------------------------------------------------------------------#
#CCOPT			=	# C++ optimization flags, default lets build config decide
#COPT			=	# C optimization flags, default lets build config decide
#SUBSYSTEM = Subsystem to build for (none, console or windows) [none]
#					 (Windows Only)
#USEMFC	= How Windows MFC should be used (none, static, shared, no_mfc) [none]
#				(Windows Only)
#=============================================================================#

#=============================================================================#
# Rules:
#-----------------------------------------------------------------------------#
# process Sub-directories
include $(TL_DIR)/Makerules/Maketargets.toplevel

# build cmds and libs
include $(TL_DIR)/Makerules/Maketargets.build

# install for includes, libs and cmds phases
include $(TL_DIR)/Makerules/Maketargets.install

# install for stage phase
include $(TL_DIR)/Makerules/Maketargets.stage
STAGE::
ifeq "$(RELEASE_TAG)" ""
	echo $(MODULEVERSION) | tr '_-' '..' > $(PROJ_STAGE_DIR)/version
else
	echo $(RELEASE_TAG) | tr '_-' '..' > $(PROJ_STAGE_DIR)/version
endif
	echo $(BUILD_TARGET) > $(PROJ_STAGE_DIR)/arch
	echo $(BUILD_TARGET_OS_VENDOR) > $(PROJ_STAGE_DIR)/distro
	echo $(BUILD_TARGET_OS_VENDOR_VERSION) > $(PROJ_STAGE_DIR)/distro_version
	echo $(BUILD_TARGET_OS_ID) > $(PROJ_STAGE_DIR)/os_id

# put version number into a file to facilitate rpm construction
# presently only INSTALL allows dynamic version patch
prepfiles:: check_env
ifeq "$(RELEASE_TAG)" ""
	$(CONVERT_RELEASETAG) $(MODULEVERSION) > $(PROJ_STAGE_DIR)/version
else
	$(CONVERT_RELEASETAG) $(RELEASE_TAG) > $(PROJ_STAGE_DIR)/version
endif
	cd $(PROJ_STAGE_DIR) && find . -print| $(PREP)
	cd $(PROJ_STAGE_DIR) && $(PATCH_VERSION) -m % $(RELEASE_TAG) INSTALL
	cd $(PROJ_STAGE_DIR) && [ ! -e fastfabric/opatest ] || $(PATCH_VERSION) -m % $(RELEASE_TAG) fastfabric/opatest fastfabric/opafastfabric
	cd $(PROJ_STAGE_DIR) && $(PATCH_BRAND) -m % "$(BUILD_BRAND)" INSTALL
	cd $(PROJ_STAGE_DIR) && [ ! -e fastfabric/opatest ] || $(PATCH_BRAND) -m % "$(BUILD_BRAND)" fastfabric/opafastfabric

# OS specific packaging step
# package builds standard package
# package_full can include extra depricated items
package package_full:
	cd $(BUILD_TARGET_OS) && $(MAKE) $(MFLAGS) $@
.PHONY:  package package_full

vars_include: vars_include_global

final_package:
	if [ -d rpmbuild ]; then \
		cd rpmbuild; \
	else \
		cd ../rpmbuild; \
	fi; \
	dir=`grep 'IntelOPA-Tools-FF\.' BUILD/opa-$${MKRPM_VER}/packaged_files |xargs dirname`; \
	filename=`grep 'IntelOPA-Tools-FF\.' BUILD/opa-$${MKRPM_VER}/packaged_files|xargs basename`; \
	subdir=$${filename%.tgz}; \
	srpmdir=$$dir/$$subdir/SRPMS; \
	basicdir=`grep 'IntelOPA-Tools\.' BUILD/opa-$${MKRPM_VER}/packaged_files |xargs dirname`; \
	basic=`grep 'IntelOPA-Tools\.' BUILD/opa-$${MKRPM_VER}/packaged_files|xargs basename`; \
	arch=`echo $(BUILD_TARGET) | tr '[A-Z]' '[a-z]'`; \
	rpmdir=$$dir/$$subdir/RPMS/$$arch;\
	basicrpmdir=$$basicdir/$${basic%.tgz}/RPMS/$$arch;\
	mkdir -p $$srpmdir; if [ $$? -ne 0 ]; then echo "ERR""OR: mkdir $$srpmdir."; fi; \
	mkdir -p $$rpmdir; if [ $$? -ne 0 ]; then echo "ERR""OR: mkdir $$rpmdir."; fi; \
	rm -rf $${basicdir}/$${basic} $${basicdir}/$${basic%.tgz}; \
	cp -rp $${dir}/$${subdir} $${basicdir}/$${basic%.tgz}; if [ $$? -ne 0 ]; then echo "ERR""OR: cp $${basicdir}/$${basic%.tgz}."; fi; \
	cp SRPMS/opa-$${MKRPM_VER}-$${MKRPM_REL}.src.rpm $$srpmdir/ ; if [ $$? -ne 0 ]; then echo "ERR""OR: cp $$srpmdir."; fi; \
	cp RPMS/$$arch/opa-basic-tools-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/; if [ $$? -ne 0 ]; then echo "ERR""OR: cp $$rpmdir."; fi; \
	cp RPMS/$$arch/opa-address-resolution-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/; if [ $$? -ne 0 ]; then echo "ERR""OR: cp $$rpmdir."; fi; \
	cp RPMS/$$arch/opa-fastfabric-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/; if [ $$? -ne 0 ]; then echo "ERR""OR: cp $$rpmdir."; fi; \
	cp RPMS/$$arch/opa-basic-tools-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$basicrpmdir/; if [ $$? -ne 0 ]; then echo "ERR""OR: cp $$basicrpmdir."; fi; \
	cp RPMS/$$arch/opa-address-resolution-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$basicrpmdir/; if [ $$? -ne 0 ]; then echo "ERR""OR: cp $$basicrpmdir."; fi; \
	if [ "$(BUILD_TARGET_OS_VENDOR)" = "redhat" ]; then \
		cp RPMS/$$arch/opa-debuginfo-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/;  \
	fi; \
	if [ "$(BUILD_TARGET_OS_VENDOR)" = "SuSE" ]; then \
		cp RPMS/$$arch/opa-debugsource-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/;  \
		cp RPMS/$$arch/opa-basic-tools-debuginfo-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/;  \
		cp RPMS/$$arch/opa-basic-tools-debuginfo-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$basicrpmdir/;  \
		cp RPMS/$$arch/opa-address-resolution-debuginfo-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/;  \
		cp RPMS/$$arch/opa-address-resolution-debuginfo-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$basicrpmdir/;  \
		cp RPMS/$$arch/opa-fastfabric-debuginfo-$${MKRPM_VER}-$${MKRPM_REL}.$$arch.rpm $$rpmdir/;  \
	fi; \
	cd $$dir; \
	echo "Packaging $$dir/$$filename ..."; \
	tar cfz $$filename $$subdir; \
	echo "Packaging $$basicdir/$$basic ..."; \
	tar cfz $${basic} --exclude mpi --exclude shmem $${basic%.tgz}; 

# Unit test execution
#include $(TL_DIR)/Makerules/Maketargets.runtest

tools:
	mkdir -p $(COMN_LIB_DIRS) $(GLOBAL_SHLIB_DIR)
	cd $(TL_DIR)/MakeTools && $(MAKE) $(MFLAGS)
clobber_tools:
	cd $(TL_DIR)/MakeTools && $(MAKE) $(MFLAGS) clobber
CLOBBER::
	rm -f packaged_files dist_files

#=============================================================================#

#=============================================================================#
# DO NOT DELETE THIS LINE -- make depend depends on it.
#=============================================================================#
