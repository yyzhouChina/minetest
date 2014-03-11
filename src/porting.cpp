/*
Minetest
Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/*
	Random portability stuff

	See comments in porting.h
*/

#if defined(__APPLE__)
	#include <mach-o/dyld.h>
	#include "CoreFoundation/CoreFoundation.h"
#elif defined(__FreeBSD__)
	#include <sys/types.h>
	#include <sys/sysctl.h>
#elif defined(_WIN32)
	#include <algorithm>
#endif
#if !defined(_WIN32)
	#include <unistd.h>
	#include <sys/utsname.h>
#endif

#include "porting.h"
#include "config.h"
#include "debug.h"
#include "filesys.h"
#include "log.h"
#include "util/string.h"
#include <list>

namespace porting
{

/*
	Signal handler (grabs Ctrl-C on POSIX systems)
*/

bool g_killed = false;

bool * signal_handler_killstatus(void)
{
	return &g_killed;
}

#if !defined(_WIN32) // POSIX
	#include <signal.h>

void sigint_handler(int sig)
{
	if(g_killed == false)
	{
		dstream<<DTIME<<"INFO: sigint_handler(): "
				<<"Ctrl-C pressed, shutting down."<<std::endl;

		// Comment out for less clutter when testing scripts
		/*dstream<<DTIME<<"INFO: sigint_handler(): "
				<<"Printing debug stacks"<<std::endl;
		debug_stacks_print();*/

		g_killed = true;
	}
	else
	{
		(void)signal(SIGINT, SIG_DFL);
	}
}

void signal_handler_init(void)
{
	(void)signal(SIGINT, sigint_handler);
}

#else // _WIN32
	#include <signal.h>

	BOOL WINAPI event_handler(DWORD sig)
	{
		switch(sig)
		{
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:

			if(g_killed == false)
			{
				dstream<<DTIME<<"INFO: event_handler(): "
						<<"Ctrl+C, Close Event, Logoff Event or Shutdown Event, shutting down."<<std::endl;
				// Comment out for less clutter when testing scripts
				/*dstream<<DTIME<<"INFO: event_handler(): "
						<<"Printing debug stacks"<<std::endl;
				debug_stacks_print();*/

				g_killed = true;
			}
			else
			{
				(void)signal(SIGINT, SIG_DFL);
			}

			break;
		case CTRL_BREAK_EVENT:
			break;
		}

		return TRUE;
	}

void signal_handler_init(void)
{
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE)event_handler,TRUE);
}

#endif


/*
	Multithreading support
*/
int getNumberOfProcessors() {
#if defined(_SC_NPROCESSORS_ONLN)

	return sysconf(_SC_NPROCESSORS_ONLN);

#elif defined(__FreeBSD__) || defined(__APPLE__)

	unsigned int len, count;
	len = sizeof(count);
	return sysctlbyname("hw.ncpu", &count, &len, NULL, 0);

#elif defined(_GNU_SOURCE)

	return get_nprocs();

#elif defined(_WIN32)

	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return sysinfo.dwNumberOfProcessors;

#elif defined(PTW32_VERSION) || defined(__hpux)

	return pthread_num_processors_np();

#else

	return 1;

#endif
}


#ifndef _IRR_ANDROID_PLATFORM_
bool threadBindToProcessor(threadid_t tid, int pnumber) {
#if defined(_WIN32)

	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, 0, tid);
	if (!hThread)
		return false;

	bool success = SetThreadAffinityMask(hThread, 1 << pnumber) != 0;

	CloseHandle(hThread);
	return success;

#elif (defined(__FreeBSD__) && (__FreeBSD_version >= 702106)) \
	|| defined(__linux) || defined(linux)

	cpu_set_t cpuset;

	CPU_ZERO(&cpuset);
	CPU_SET(pnumber, &cpuset);
	return pthread_setaffinity_np(tid, sizeof(cpuset), &cpuset) == 0;

#elif defined(__sun) || defined(sun)

	return processor_bind(P_LWPID, MAKE_LWPID_PTHREAD(tid),
						pnumber, NULL) == 0;

#elif defined(_AIX)

	return bindprocessor(BINDTHREAD, (tid_t)tid, pnumber) == 0;

#elif defined(__hpux) || defined(hpux)

	pthread_spu_t answer;

	return pthread_processor_bind_np(PTHREAD_BIND_ADVISORY_NP,
									&answer, pnumber, tid) == 0;

#elif defined(__APPLE__)

	struct thread_affinity_policy tapol;

	thread_port_t threadport = pthread_mach_thread_np(tid);
	tapol.affinity_tag = pnumber + 1;
	return thread_policy_set(threadport, THREAD_AFFINITY_POLICY,
			(thread_policy_t)&tapol, THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS;

#else

	return false;

#endif
}
#endif

bool threadSetPriority(threadid_t tid, int prio) {
#if defined(_WIN32)

	HANDLE hThread = OpenThread(THREAD_ALL_ACCESS, 0, tid);
	if (!hThread)
		return false;

	bool success = SetThreadPriority(hThread, prio) != 0;

	CloseHandle(hThread);
	return success;

#else

	struct sched_param sparam;
	int policy;

	if (pthread_getschedparam(tid, &policy, &sparam) != 0)
		return false;

	int min = sched_get_priority_min(policy);
	int max = sched_get_priority_max(policy);

	sparam.sched_priority = min + prio * (max - min) / THREAD_PRIORITY_HIGHEST;
	return pthread_setschedparam(tid, policy, &sparam) == 0;

#endif
}


/*
	Path mangler
*/

// Default to RUN_IN_PLACE style relative paths
std::string path_share = "..";
std::string path_user = "..";

std::string getDataPath(const char *subpath)
{
	return path_share + DIR_DELIM + subpath;
}

void pathRemoveFile(char *path, char delim)
{
	// Remove filename and path delimiter
	int i;
	for(i = strlen(path)-1; i>=0; i--)
	{
		if(path[i] == delim)
			break;
	}
	path[i] = 0;
}

bool detectMSVCBuildDir(char *c_path)
{
	std::string path(c_path);
	const char *ends[] = {"bin\\Release", "bin\\Build", NULL};
	return (removeStringEnd(path, ends) != "");
}

std::string get_sysinfo()
{
#ifdef _WIN32
	OSVERSIONINFO osvi;
	std::ostringstream oss;
	std::string tmp;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);
	tmp = osvi.szCSDVersion;
	std::replace(tmp.begin(), tmp.end(), ' ', '_');

	oss << "Windows/" << osvi.dwMajorVersion << "."
		<< osvi.dwMinorVersion;
	if(osvi.szCSDVersion[0])
		oss << "-" << tmp;
	oss << " ";
	#ifdef _WIN64
	oss << "x86_64";
	#else
	BOOL is64 = FALSE;
	if(IsWow64Process(GetCurrentProcess(), &is64) && is64)
		oss << "x86_64"; // 32-bit app on 64-bit OS
	else
		oss << "x86";
	#endif

	return oss.str();
#else
	struct utsname osinfo;
	uname(&osinfo);
	return std::string(osinfo.sysname) + "/"
		+ osinfo.release + " " + osinfo.machine;
#endif
}

void initializePaths()
{
#if RUN_IN_PLACE
	/*
		Use relative paths if RUN_IN_PLACE
	*/

	infostream<<"Using relative paths (RUN_IN_PLACE)"<<std::endl;

	/*
		Windows
	*/
	#if defined(_WIN32)

	const DWORD buflen = 1000;
	char buf[buflen];
	DWORD len;

	// Find path of executable and set path_share relative to it
	len = GetModuleFileName(GetModuleHandle(NULL), buf, buflen);
	assert(len < buflen);
	pathRemoveFile(buf, '\\');

	if(detectMSVCBuildDir(buf)){
		infostream<<"MSVC build directory detected"<<std::endl;
		path_share = std::string(buf) + "\\..\\..";
		path_user = std::string(buf) + "\\..\\..";
	}
	else{
		path_share = std::string(buf) + "\\..";
		path_user = std::string(buf) + "\\..";
	}

	/*
		Linux
	*/
	#elif defined(linux)

	char buf[BUFSIZ];
	memset(buf, 0, BUFSIZ);
	// Get path to executable
	assert(readlink("/proc/self/exe", buf, BUFSIZ-1) != -1);

	pathRemoveFile(buf, '/');

	path_share = std::string(buf) + "/..";
	path_user = std::string(buf) + "/..";

	/*
		OS X
	*/
	#elif defined(__APPLE__)

	//https://developer.apple.com/library/mac/#documentation/Darwin/Reference/ManPages/man3/dyld.3.html
	//TODO: Test this code
	char buf[BUFSIZ];
	uint32_t len = sizeof(buf);
	assert(_NSGetExecutablePath(buf, &len) != -1);

	pathRemoveFile(buf, '/');

	path_share = std::string(buf) + "/..";
	path_user = std::string(buf) + "/..";

	/*
		FreeBSD
	*/
	#elif defined(__FreeBSD__)

	int mib[4];
	char buf[BUFSIZ];
	size_t len = sizeof(buf);

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PATHNAME;
	mib[3] = -1;
	assert(sysctl(mib, 4, buf, &len, NULL, 0) != -1);

	pathRemoveFile(buf, '/');

	path_share = std::string(buf) + "/..";
	path_user = std::string(buf) + "/..";

	#else

	//TODO: Get path of executable. This assumes working directory is bin/
	dstream<<"WARNING: Relative path not properly supported on this platform"
			<<std::endl;

	/* scriptapi no longer allows paths that start with "..", so assuming that
	   the current working directory is bin/, strip off the last component. */
	char *cwd = getcwd(NULL, 0);
	pathRemoveFile(cwd, '/');
	path_share = std::string(cwd);
	path_user = std::string(cwd);

	#endif

#else // RUN_IN_PLACE

	/*
		Use platform-specific paths otherwise
	*/

	infostream<<"Using system-wide paths (NOT RUN_IN_PLACE)"<<std::endl;

	/*
		Windows
	*/
	#if defined(_WIN32)

	const DWORD buflen = 1000;
	char buf[buflen];
	DWORD len;

	// Find path of executable and set path_share relative to it
	len = GetModuleFileName(GetModuleHandle(NULL), buf, buflen);
	assert(len < buflen);
	pathRemoveFile(buf, '\\');

	// Use ".\bin\.."
	path_share = std::string(buf) + "\\..";

	// Use "C:\Documents and Settings\user\Application Data\<PROJECT_NAME>"
	len = GetEnvironmentVariable("APPDATA", buf, buflen);
	assert(len < buflen);
	path_user = std::string(buf) + DIR_DELIM + PROJECT_NAME;

	/*
		Linux
	*/
	#elif defined(linux)

	// Get path to executable
	std::string bindir = "";
	{
		char buf[BUFSIZ];
		memset(buf, 0, BUFSIZ);
		assert(readlink("/proc/self/exe", buf, BUFSIZ-1) != -1);
		pathRemoveFile(buf, '/');
		bindir = buf;
	}

	// Find share directory from these.
	// It is identified by containing the subdirectory "builtin".
	std::list<std::string> trylist;
	std::string static_sharedir = STATIC_SHAREDIR;
	if(static_sharedir != "" && static_sharedir != ".")
		trylist.push_back(static_sharedir);
	trylist.push_back(
			bindir + DIR_DELIM + ".." + DIR_DELIM + "share" + DIR_DELIM + PROJECT_NAME);
	trylist.push_back(bindir + DIR_DELIM + "..");
#ifdef __ANDROID__
	trylist.push_back(DIR_DELIM "sdcard" DIR_DELIM PROJECT_NAME);
#endif

	for(std::list<std::string>::const_iterator i = trylist.begin();
			i != trylist.end(); i++)
	{
		const std::string &trypath = *i;
		if(!fs::PathExists(trypath) || !fs::PathExists(trypath + DIR_DELIM + "builtin")){
			dstream<<"WARNING: system-wide share not found at \""
					<<trypath<<"\""<<std::endl;
			continue;
		}
		// Warn if was not the first alternative
		if(i != trylist.begin()){
			dstream<<"WARNING: system-wide share found at \""
					<<trypath<<"\""<<std::endl;
		}
		path_share = trypath;
		break;
	}

	infostream << "something something " PROJECT_NAME << std::endl;

#ifndef __ANDROID__
	path_user = std::string(getenv("HOME")) + DIR_DELIM + "." + PROJECT_NAME;
#else
	path_user = std::string(DIR_DELIM "sdcard" DIR_DELIM PROJECT_NAME DIR_DELIM);
#endif


	infostream << path_user << std::endl;

	/*
		OS X
	*/
	#elif defined(__APPLE__)

	// Code based on
	// http://stackoverflow.com/questions/516200/relative-paths-not-working-in-xcode-c
	CFBundleRef main_bundle = CFBundleGetMainBundle();
	CFURLRef resources_url = CFBundleCopyResourcesDirectoryURL(main_bundle);
	char path[PATH_MAX];
	if(CFURLGetFileSystemRepresentation(resources_url, TRUE, (UInt8 *)path, PATH_MAX))
	{
		dstream<<"Bundle resource path: "<<path<<std::endl;
		//chdir(path);
		path_share = std::string(path) + DIR_DELIM + "share";
	}
	else
	{
		// error!
		dstream<<"WARNING: Could not determine bundle resource path"<<std::endl;
	}
	CFRelease(resources_url);

	path_user = std::string(getenv("HOME")) + "/Library/Application Support/" + PROJECT_NAME;

	#else // FreeBSD, and probably many other POSIX-like systems.

	path_share = STATIC_SHAREDIR;
	path_user = std::string(getenv("HOME")) + DIR_DELIM + "." + PROJECT_NAME;

	#endif

#endif // RUN_IN_PLACE
}

#ifdef __ANDROID__
std::string path_storage = DIR_DELIM "sdcard" DIR_DELIM;


// http://stackoverflow.com/questions/5864790/how-to-show-the-soft-keyboard-on-native-activity
void displayKeyboard(bool pShow, android_app* mApplication, JNIEnv* lJNIEnv) {
    jint lFlags = 0;

    // Retrieves NativeActivity.
    jobject lNativeActivity = mApplication->activity->clazz;
    jclass ClassNativeActivity = lJNIEnv->GetObjectClass(lNativeActivity);

    // Retrieves Context.INPUT_METHOD_SERVICE.
    jclass ClassContext = lJNIEnv->FindClass("android/content/Context");
    jfieldID FieldINPUT_METHOD_SERVICE = lJNIEnv->GetStaticFieldID(ClassContext, "INPUT_METHOD_SERVICE", "Ljava/lang/String;");
    jobject INPUT_METHOD_SERVICE = lJNIEnv->GetStaticObjectField(ClassContext, FieldINPUT_METHOD_SERVICE);
    //jniCheck(INPUT_METHOD_SERVICE);

    // Runs getSystemService(Context.INPUT_METHOD_SERVICE).
    jclass ClassInputMethodManager = lJNIEnv->FindClass("android/view/inputmethod/InputMethodManager");
    jmethodID MethodGetSystemService = lJNIEnv->GetMethodID(ClassNativeActivity, "getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
    jobject lInputMethodManager = lJNIEnv->CallObjectMethod(lNativeActivity, MethodGetSystemService,INPUT_METHOD_SERVICE);

    // Runs getWindow().getDecorView().
    jmethodID MethodGetWindow = lJNIEnv->GetMethodID(ClassNativeActivity, "getWindow","()Landroid/view/Window;");
    jobject lWindow = lJNIEnv->CallObjectMethod(lNativeActivity,MethodGetWindow);
    jclass ClassWindow = lJNIEnv->FindClass("android/view/Window");
    jmethodID MethodGetDecorView = lJNIEnv->GetMethodID(ClassWindow, "getDecorView", "()Landroid/view/View;");
    jobject lDecorView = lJNIEnv->CallObjectMethod(lWindow,MethodGetDecorView);

    if (pShow) {
        // Runs lInputMethodManager.showSoftInput(...).
        jmethodID MethodShowSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager, "showSoftInput","(Landroid/view/View;I)Z");
        jboolean lResult = lJNIEnv->CallBooleanMethod(lInputMethodManager, MethodShowSoftInput,lDecorView, lFlags);
    } else {
        // Runs lWindow.getViewToken()
        jclass ClassView = lJNIEnv->FindClass("android/view/View");
        jmethodID MethodGetWindowToken = lJNIEnv->GetMethodID(ClassView, "getWindowToken", "()Landroid/os/IBinder;");
        jobject lBinder = lJNIEnv->CallObjectMethod(lDecorView,MethodGetWindowToken);

        // lInputMethodManager.hideSoftInput(...).
        jmethodID MethodHideSoftInput = lJNIEnv->GetMethodID(ClassInputMethodManager, "hideSoftInputFromWindow","(Landroid/os/IBinder;I)Z");
        jboolean lRes = lJNIEnv->CallBooleanMethod(lInputMethodManager, MethodHideSoftInput,lBinder, lFlags);
    }
}

android_app* app_global;
JNIEnv* jnienv;
jclass nativeActivity;

jclass findClass(std::string classname)
{
	if (jnienv == 0) {
		return 0;
	}

	jclass nativeactivity = jnienv->FindClass("android/app/NativeActivity");
	jmethodID getClassLoader = jnienv->GetMethodID(nativeactivity,"getClassLoader", "()Ljava/lang/ClassLoader;");
	jobject cls = jnienv->CallObjectMethod(app_global->activity->clazz, getClassLoader);
	jclass classLoader = jnienv->FindClass("java/lang/ClassLoader");
	jmethodID findClass = jnienv->GetMethodID(classLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	jstring strClassName = jnienv->NewStringUTF(classname.c_str());
	return (jclass) jnienv->CallObjectMethod(cls, findClass, strClassName);
}

void copyAssets() {
	jmethodID assetcopy = jnienv->GetMethodID(nativeActivity,"copyAssets","()V");

	if (assetcopy == 0) {
		assert("porting::copyAssets unable to find copy assets method" == 0);
	}

	jnienv->CallVoidMethod(app_global->activity->clazz, assetcopy);
}

void initAndroid()
{
	porting::jnienv = NULL;
	JavaVM *jvm = app_global->activity->vm;
	JavaVMAttachArgs lJavaVMAttachArgs;
	lJavaVMAttachArgs.version = JNI_VERSION_1_6;
//	lJavaVMAttachArgs.name = "NativeThread";
	lJavaVMAttachArgs.group = NULL;
	jvm->AttachCurrentThread(&jnienv, &lJavaVMAttachArgs);

	nativeActivity = findClass("org/minetest/MtNativeActivity");
	if (nativeActivity == 0) {
		errorstream <<
			"porting::initAndroid unable to find java native activity class" <<
			std::endl;
	}
}

void setExternalStorageDir(JNIEnv* lJNIEnv) {

	// Android: Retrieve ablsolute path to external storage device (sdcard)
	jclass ClassEnv = lJNIEnv->FindClass("android/os/Environment");
	jmethodID MethodDir = lJNIEnv->GetStaticMethodID(ClassEnv, "getExternalStorageDirectory","()Ljava/io/File;");
	jobject ObjectFile = lJNIEnv->CallStaticObjectMethod(ClassEnv, MethodDir);
	jclass ClassFile = lJNIEnv->FindClass("java/io/File");

	jmethodID MethodPath = lJNIEnv->GetMethodID(ClassFile, "getAbsolutePath", "()Ljava/lang/String;");
	jstring StringPath = (jstring)lJNIEnv->CallObjectMethod(ObjectFile, MethodPath);

	const char *externalPath = lJNIEnv->GetStringUTFChars(StringPath, NULL);
	std::string userPath(externalPath);
	lJNIEnv->ReleaseStringUTFChars(StringPath, externalPath);

	path_storage = userPath;
	path_user = userPath + DIR_DELIM + PROJECT_NAME;
	path_share = userPath + DIR_DELIM + PROJECT_NAME;
}

}// namespace porting

namespace porting
{

void showInputDialog(const std::string& acceptButton, const  std::string& hint,
		const std::string& current, int editType)
{
	jmethodID showdialog = jnienv->GetMethodID(nativeActivity,"showDialog",
		"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");

	if (showdialog == 0) {
		assert("porting::showInputDialog unable to find java show dialog method" == 0);
	}

	jstring jacceptButton = jnienv->NewStringUTF(acceptButton.c_str());
	jstring jhint         = jnienv->NewStringUTF(hint.c_str());
	jstring jcurrent      = jnienv->NewStringUTF(current.c_str());
	jint    jeditType     = editType;

	jnienv->CallVoidMethod(app_global->activity->clazz, showdialog,
			jacceptButton, jhint, jcurrent, jeditType);
}

int getInputDialogState() {
	jmethodID dialogstate = jnienv->GetMethodID(nativeActivity,"getDialogState",
			"()I");

	if (dialogstate == 0) {
		assert("porting::getInputDialogState unable to find java dialog state method" == 0);
	}

	return jnienv->CallIntMethod(app_global->activity->clazz,dialogstate);
}

std::string getInputDialogValue() {
	jmethodID dialogvalue = jnienv->GetMethodID(nativeActivity,"getDialogValue",
			"()Ljava/lang/String;");

	if (dialogvalue == 0) {
		assert("porting::getInputDialogValue unable to find java dialog value method" == 0);
	}

	jobject result = jnienv->CallObjectMethod(app_global->activity->clazz,dialogvalue);

	const char* javachars = jnienv->GetStringUTFChars((jstring) result,0);
	std::string text(javachars);
	jnienv->ReleaseStringUTFChars((jstring) result,javachars);

	return text;
}

#endif

} //namespace porting

#ifdef __ANDROID__
extern "C" {
	JNIEXPORT void JNICALL Java_org_minetest_MtNativeActivity_putMessageBoxResult(
			JNIEnv * env, jclass thiz, jstring text)
	{
		errorstream << "Java_org_minetest_MtNativeActivity_putMessageBoxResult got: "
				<< std::string((const char*)env->GetStringChars(text,0)) << std::endl;
	}
}
#endif
