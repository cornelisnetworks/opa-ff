# BEGIN_ICS_COPYRIGHT8 ****************************************
# 
# Copyright (c) 2015-2017, Intel Corporation
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
# Makefile for IbaTools/man

# Include Make Control Settings                                                 
include $(TL_DIR)/$(PROJ_FILE_DIR)/Makesettings.project                         

#=============================================================================#
# Definitions:
#-----------------------------------------------------------------------------#

# Name of SubProjects
DS_SUBPROJECTS	= 
# name of executable or downloadable image
EXECUTABLE		= 
# list of sub directories to build
DIRS			= 
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
LIBFILES=
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
MAN_PAGE_1_FILES = \
				opacapture.1 \
				opaconfig.1 \
				opafabricinfo.1 \
				opagetvf.1 \
				opagetvf_env.1 \
				opahfirev.1 \
				opainfo.1 \
				opa_osd_dump.1 \
				opa_osd_exercise.1 \
				opa_osd_perf.1 \
				opa_osd_query.1 \
				opapacketcapture.1 \
				opapmaquery.1 \
				opaportconfig.1 \
				opaportinfo.1 \
				oparesolvehfiport.1 \
				opasaquery.1 \
				opashowmc.1 \
				opasmaquery.1 \
				# Add more man pages for chapter 1
MAN_PAGE_8_FILES = \
				opaallanalysis.8 \
				opacabletest.8 \
				opacaptureall.8 \
				opachassisadmin.8 \
				opachassisanalysis.8 \
				opacheckload.8 \
				opacmdall.8 \
				opadisablehosts.8 \
				opadisableports.8 \
				opadownloadall.8 \
				opaenableports.8 \
				opaledports.8 \
				opaesmanalysis.8 \
				opaexpandfile.8 \
				opaextractbadlinks.8 \
				opaextracterror.8 \
				opaextractlids.8 \
				opaextractlink.8 \
				opaextractperf.8 \
				opaextractsellinks.8 \
				opaextractstat.8 \
				opaextractstat2.8 \
				opafabricanalysis.8 \
				opafastfabric.8 \
				opafindgood.8 \
				opafirmware.8 \
				opafmconfigcheck.8 \
				opafmconfigdiff.8 \
				opagenchassis.8 \
				opagenesmchassis.8 \
				opagenswitches.8 \
				opagentopology.8 \
				opahostadmin.8 \
				opahostsmanalysis.8 \
				opalinkanalysis.8 \
				opapaquery.8 \
				opapingall.8 \
				opareport.8 \
				opareports.8 \
				opascpall.8 \
				opasetupssh.8 \
				opashowallports.8 \
				opasorthosts.8 \
				opaswitchadmin.8 \
				opatop.8 \
				opauploadall.8 \
				opaverifyhosts.8 \
				opaxlattopology.8 \
				opashowmc.8 \
				opaxmlextract.8 \
				opaxmlfilter.8 \
				opaxmlgenerate.8 \
				opaxmlindent.8 \
				opaswdisableall.8 \
				opaswenableall.8 \
				opaextractmissinglinks.8 \
				opa2rm.8 \
				# Add more man pages for chapter 8
MISC_FILES= \
				$(MAN_PAGE_1_FILES) \
				$(MAN_PAGE_8_FILES) \
				# Add more files here
# all source files
SOURCES			= $(CFILES) $(CCFILES) $(LFILES) $(RSCFILES) $(IDLFILE)
# Source files to include in DSP File
DSP_SOURCES		= $(INCLUDE_TARGETS) $(SOURCES) $(MISC_FILES) \
				  $(RSCFILES) $(DEFFILE) $(MAKEFILE) 
# all object files
OBJECTS			= $(CFILES:.c=$(OBJ_SUFFIX)) $(CCFILES:.cpp=$(OBJ_SUFFIX)) \
				  $(LFILES:.lex=$(OBJ_SUFFIX))
RSCOBJECTS		= $(RSCFILES:.rc=$(RES_SUFFIX))
# targets to build during LIBS phase
LIB_TARGETS_IMPLIB	=
LIB_TARGETS_ARLIB	= # $(LIB_PREFIX)ResourceTest$(ARLIB_SUFFIX)
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

CLOCAL = $(CPIE)
LOCAL_INCLUDE_DIRS=
LOCALDEPLIBS=
LOCALLIBS=
LOCAL_LIB_DIRS=

# Include Make Rules definitions and rules
include $(TL_DIR)/IbaTools/Makerules.module

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
#include $(TL_DIR)/Makerules/Maketargets.stage
STAGE::
	$(VS)$(STAGE_INSTALL) $(STAGE_INSTALL_DIR_OPT) $(PROJ_STAGE_MAN_DIR)/man1 $(MAN_PAGE_1_FILES)
	$(VS)$(STAGE_INSTALL) $(STAGE_INSTALL_DIR_OPT) $(PROJ_STAGE_MAN_DIR)/man8 $(MAN_PAGE_8_FILES)

# Unit test execution
#include $(TL_DIR)/Makerules/Maketargets.runtest

#=============================================================================#

#=============================================================================#
# DO NOT DELETE THIS LINE -- make depend depends on it.
#=============================================================================#
