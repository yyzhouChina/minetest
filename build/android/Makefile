# build options

# how much threads to use for build
PARALLEL = -j4

# compile with GPROF
GPROF = 1

# paths used for timestaps, dependencys, tree config and libs
PATHCFGFILE = path.cfg

ROOT = $(shell pwd)

ASSETS_TIMESTAMP = deps/assets_timestamp

LEVELDB_DIR = $(ROOT)/deps/leveldb/
LEVELDB_LIB = $(LEVELDB_DIR)libleveldb.a
LEVELDB_TIMESTAMP = $(LEVELDB_DIR)timestamp
LEVELDB_TIMESTAMP_INT = $(ROOT)/deps/leveldb_timestamp

IRRLICHT_DIR = $(ROOT)/deps/irrlicht/
IRRLICHT_LIB = $(IRRLICHT_DIR)lib/Android/libIrrlicht.a
IRRLICHT_TIMESTAMP = $(IRRLICHT_DIR)timestamp
IRRLICHT_TIMESTAMP_INT = $(ROOT)/deps/irrlicht_timestamp

-include $(PATHCFGFILE)

.PHONY : debug release reconfig delconfig \
	leveldb_download clean_leveldb leveldb\
	irrlicht_download clean_irrlicht irrlicht \
	clean_assets assets $(ASSETS_TIMESTAMP) \
	apk_debug apk_release clean_apk \
	clean_all clean prep_srcdir \
	install_debug install envpaths all

debug : $(PATHCFGFILE) 
	make apk_debug
	
all : debug release
	
release : $(PATHCFGFILE)
	make apk_release

reconfig: delconfig
	make $(PATHCFGFILE)

delconfig :
	$(RM) ${PATHCFGFILE}

$(PATHCFGFILE) :
	@echo "Please specify path of ANDROID NDK";                                \
	echo "e.g. /home/user/android-ndk-r9c/";                                   \
	read ANDROID_NDK ;                                                         \
	if [ ! -d $$ANDROID_NDK ] ; then                                           \
		echo "$$ANDROID_NDK is not a valid folder";                            \
		exit 1;                                                                \
	fi;                                                                        \
	echo "ANDROID_NDK = $$ANDROID_NDK" > ${PATHCFGFILE};                       \
	echo "NDK_MODULE_PATH = $$ANDROID_NDK/tools" >> ${PATHCFGFILE};            \
	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";\
	echo "+ Note: NDK_MODULE_PATH is set to $$ANDROID_NDK/tools";              \
	echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++";\
	echo "Please specify path of ANDROID SDK";                                 \
	echo "e.g. /home/user/adt-bundle-linux-x86_64-20131030/sdk/";              \
	read SDKFLDR ;                                                             \
	if [ ! -d $$SDKFLDR ] ; then                                               \
		echo "$$SDKFLDR is not a valid folder";                                \
		exit 1;                                                                \
	fi;                                                                        \
	echo "SDKFOLDER = $$SDKFLDR" >> ${PATHCFGFILE};



$(LEVELDB_TIMESTAMP) : leveldb_download
	@LAST_MODIF=$$(find ${LEVELDB_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${LEVELDB_TIMESTAMP};                                            \
	fi

leveldb_download :
	@if [ ! -d ${LEVELDB_DIR} ] ; then                                         \
		echo "leveldb sources missing, downloading...";                        \
		mkdir -p ${ROOT}/deps;                                                 \
		cd ${ROOT}/deps ;                                                      \
		git clone https://code.google.com/p/leveldb/ || exit 1;                \
	fi
	
leveldb : $(LEVELDB_LIB)

$(LEVELDB_LIB): $(LEVELDB_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${LEVELDB_TIMESTAMP_INT} ] ; then                                \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${LEVELDB_TIMESTAMP} -nt ${LEVELDB_TIMESTAMP_INT} ] ; then            \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	echo "changed timestamp for leveldb detected building...";                 \
	cd deps/leveldb;                                                           \
	export CROSS_PREFIX=arm-linux-androideabi-;                                \
	export TOOLCHAIN=/tmp/ndk-arm;                                             \
	${ANDROID_NDK}/build/tools/make-standalone-toolchain.sh                    \
	--toolchain=$${CROSS_PREFIX}4.6 --install-dir=$${TOOLCHAIN}                \
	--system=linux-x86_64;                                                     \
	export PATH=$$TOOLCHAIN/bin:$$PATH;                                        \
	export CC=arm-linux-androideabi-gcc;                                       \
	export CXX=arm-linux-androideabi-g++;                                      \
	export TARGET_OS=OS_ANDROID_CROSSCOMPILE;                                  \
	make -s ${PARALLEL};                                                       \
	touch ${LEVELDB_TIMESTAMP};                                        \
	touch ${LEVELDB_TIMESTAMP_INT};                                    \
	else                                                                       \
		echo "nothing to be done for leveldb";                                 \
	fi
	
clean_leveldb :
	$(RM) -rf deps/leveldb
	
$(IRRLICHT_TIMESTAMP) : irrlicht_download
	@LAST_MODIF=$$(find ${IRRLICHT_DIR} -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f2- -d" "); \
	if [ $$(basename $$LAST_MODIF) != "timestamp" ] ; then                     \
		touch ${IRRLICHT_TIMESTAMP};                                           \
	fi

irrlicht_download :
	@if [ ! -d "deps/irrlicht" ] ; then                                        \
		echo "irrlicht sources missing, downloading...";                       \
		cd deps;                                                               \
		svn co http://svn.code.sf.net/p/irrlicht/code/branches/ogl-es/ irrlicht || exit 1;\
		cd irrlicht;                                                           \
		patch -p1 < ../../irrlicht.patch || exit 1;                            \
	fi

irrlicht : $(IRRLICHT_LIB)

$(IRRLICHT_LIB): $(IRRLICHT_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${IRRLICHT_TIMESTAMP_INT} ] ; then                               \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ! -e ${IRRLICHT_LIB} ] ; then                                         \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${IRRLICHT_TIMESTAMP} -nt ${IRRLICHT_TIMESTAMP_INT} ] ; then          \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	mkdir -p ${IRRLICHT_DIR};                                                  \
	export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};             \
	echo "changed timestamp for irrlicht detected building...";                \
	cd deps/irrlicht/source/Irrlicht/Android;                                  \
	ndk-build NDEBUD=${NDEBUG} NDK_MODULE_PATH=${NDK_MODULE_PATH} ${PARALLEL}; \
	touch ${IRRLICHT_TIMESTAMP};                                               \
	touch ${IRRLICHT_TIMESTAMP_INT};                                           \
	else                                                                       \
		echo "nothing to be done for irrlicht";                                \
	fi
	
clean_irrlicht :
	$(RM) -rf deps/irrlicht

$(ASSETS_TIMESTAMP) :
	@mkdir -p ${ROOT}/deps;                                                     \
	if [ $$(ls -Art ${ROOT}/../../builtin | tail -n 1) != "timestamp" ] ; then\
		touch ${ROOT}/../../builtin/timestamp;                                 \
		touch ${ASSETS_TIMESTAMP};                                             \
		echo builtin changed;                                                  \
	fi;                                                                        \
	if [ $$(ls -Art ${ROOT}/../../client | tail -n 1) != "timestamp" ] ; then  \
		touch ${ROOT}/../../client/timestamp;                                  \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ $$(ls -Art ${ROOT}/../../doc | tail -n 1) != "timestamp" ] ; then     \
		touch ${ROOT}/../../doc/timestamp;                                     \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ $$(ls -Art ${ROOT}/../../fonts | tail -n 1) != "timestamp" ] ; then   \
		touch ${ROOT}/../../fonts/timestamp;                                   \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ $$(ls -Art ${ROOT}/../../games | tail -n 1) != "timestamp" ] ; then   \
		touch ${ROOT}/../../games/timestamp;                                   \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ $$(ls -Art ${ROOT}/../../mods | tail -n 1) != "timestamp" ] ; then    \
		touch ${ROOT}/../../mods/timestamp;                                    \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ $$(ls -Art ${ROOT}/../../po | tail -n 1) != "timestamp" ] ; then      \
		touch ${ROOT}/../../po/timestamp;                                      \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ $$(ls -Art ${ROOT}/../../textures | tail -n 1) != "timestamp" ] ; then\
		touch ${ROOT}/../../textures/timestamp;                                \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ ${ROOT}/../../minetest.conf.example -nt ${ASSETS_TIMESTAMP} ] ; then  \
		echo "conf changed";                                                   \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ ${ROOT}/../../README.txt -nt ${ASSETS_TIMESTAMP} ] ; then             \
		touch ${ASSETS_TIMESTAMP};                                             \
	fi;                                                                        \
	if [ ! -e $(ASSETS_TIMESTAMP) ] ; then                                     \
		touch $(ASSETS_TIMESTAMP);                                             \
	fi
	
assets : $(ASSETS_TIMESTAMP)
	@REFRESH=0;                                                                \
	if [ ! -e ${ASSETS_TIMESTAMP}.old ] ; then                                 \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ ${ASSETS_TIMESTAMP} -nt ${ASSETS_TIMESTAMP}.old ] ; then              \
		REFRESH=1;                                                             \
	fi;                                                                        \
	if [ $$REFRESH -ne 0 ] ; then                                              \
	echo "assets changed, refreshing...";                                      \
	make clean_assets;                                                         \
	mkdir -p ${ROOT}/assets/Minetest;                                          \
	cp ${ROOT}/../../minetest.conf.example ${ROOT}/assets/Minetest;            \
	cp ${ROOT}/../../README.txt ${ROOT}/assets/Minetest;                       \
	cp -r ${ROOT}/../../builtin ${ROOT}/assets/Minetest;                       \
	cp -r ${ROOT}/../../client ${ROOT}/assets/Minetest;                        \
	cp -r ${ROOT}/../../doc ${ROOT}/assets/Minetest;                           \
	cp -r ${ROOT}/../../fonts ${ROOT}/assets/Minetest;                         \
	cp -r ${ROOT}/../../games ${ROOT}/assets/Minetest;                         \
	cp -r ${ROOT}/../../mods ${ROOT}/assets/Minetest;                          \
	cp -r ${ROOT}/../../po ${ROOT}/assets/Minetest;                            \
	cp -r ${ROOT}/../../textures ${ROOT}/assets/Minetest;                      \
	cd ${ROOT}/assets;                                                         \
	find . -name "timestamp" -exec rm {} \; ;                                  \
	ls -R | grep ":$$" | sed -e 's/:$$//' -e 's/\.//' -e 's/^\///' > "index.txt"; \
	cp ${ROOT}/${ASSETS_TIMESTAMP} ${ROOT}/${ASSETS_TIMESTAMP}.old;            \
	fi

clean_assets :
	@$(RM) -r assets
	
apk_debug: $(PATHCFGFILE) assets $(LEVELDB_LIB) irrlicht prep_srcdir
	@export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};            \
	export ANDROID_HOME=${SDKFOLDER};                                          \
	mkdir -p ${ROOT}/src;                                                      \
	ndk-build NDK_MODULE_PATH=${NDK_MODULE_PATH}                               \
	NDEBUG=${NDEBUG} GPROF=${GPROF} ${PARALLEL} &&                             \
	ant debug &&                                                               \
	echo "++ Success!" &&                                                      \
	echo "APK: bin/Minetest-debug.apk" &&                                      \
	echo "You can install it with \`adb install -r bin/Minetest-debug.apk\`"

apk_release: $(PATHCFGFILE) assets $(LEVELDB_LIB) irrlicht prep_srcdir
	@export PATH=$$PATH:${SDKFOLDER}/platform-tools:${ANDROID_NDK};            \
	export ANDROID_HOME=${SDKFOLDER};                                          \
	mkdir -p ${ROOT}/src;                                                      \
	ndk-build NDK_MODULE_PATH=${NDK_MODULE_PATH}                               \
	NDEBUG= GPROF= ${PARALLEL} &&                                              \
	ant release &&                                                             \
	echo "++ Success!" &&                                                      \
	echo "APK: bin/Minetest.apk" &&                                            \
	echo "You can install it with \`adb install -r bin/Minetest.apk\`"
	
prep_srcdir :
	@rm ${ROOT}/jni/src;                                                       \
	ln -s ${ROOT}/../../src ${ROOT}/jni/src
	
clean_apk :
	@export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK};             \
	ndk-build clean
	
install_debug : 
	@export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK};             \
	adb install -r ${ROOT}/bin/Minetest-debug.apk
	
install :
	@export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK};             \
	adb install -r ${ROOT}/bin/Minetest.apk
	
envpaths :
	@echo "export PATH=$$PATH:${SDKFOLDER}platform-tools:${ANDROID_NDK}" > and_env;
	
clean_all : 
	make clean_apk;                                                            \
	make clean_assets clean_irrlicht clean_leveldb;                            \
	sleep 1;                                                                   \
	$(RM) -r gen libs obj deps bin Debug and_env

clean : clean_apk clean_assets