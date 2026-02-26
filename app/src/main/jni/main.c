#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

static void* g_UnityHandle = NULL;
static void* g_LdqcmbnHandle = NULL;

static jboolean load(JNIEnv* env, jclass clazz, jstring native_library_dir) {
    if (g_UnityHandle) return JNI_TRUE;
    JavaVM* vm = NULL;
    if ((*env)->GetJavaVM(env, &vm) != JNI_OK || !vm) {
        (*env)->FatalError(env, "Unable to retrieve Java VM");
        return JNI_FALSE;
    }
    const char* dirChars = (*env)->GetStringUTFChars(env, native_library_dir, NULL);
    if (!dirChars) return JNI_FALSE;
    char* dir = strdup(dirChars);
    (*env)->ReleaseStringUTFChars(env, native_library_dir, dirChars);
    if (!dir) return JNI_FALSE;
    char path[2048];
    snprintf(path, sizeof(path), "%s/%s", dir, "libunity.so");
    void* handle = dlopen(path, RTLD_LAZY);
    if (!handle) handle = dlopen("libunity.so", RTLD_LAZY);
    if (!handle) {
        char msg[1024];
        const char* err = dlerror();
        snprintf(msg, sizeof(msg), "Unable to load library: %s [%s]", path, err ? err : "unknown");
        (*env)->FatalError(env, msg);
        free(dir);
        return JNI_FALSE;
    }
    jint (*onLoad)(JavaVM*, void*) = (jint (*)(JavaVM*, void*))dlsym(handle, "JNI_OnLoad");
    if (onLoad) {
        jint ver = onLoad(vm, NULL);
        if (ver >= 0x00010007) {
            dlclose(handle);
            (*env)->FatalError(env, "Unsupported VM version");
            free(dir);
            return JNI_FALSE;
        }
    }
    g_UnityHandle = handle;
    snprintf(path, sizeof(path), "%s/%s", dir, "libldqcmbn.so");
    void* handle2 = dlopen(path, RTLD_LAZY);
    if (!handle2) handle2 = dlopen("libldqcmbn.so", RTLD_LAZY);
    if (handle2) {
        jint (*onLoad2)(JavaVM*, void*) = (jint (*)(JavaVM*, void*))dlsym(handle2, "JNI_OnLoad");
        if (onLoad2) {
            jint ver2 = onLoad2(vm, NULL);
            if (ver2 >= 0x00010007) {
                dlclose(handle2);
            } else {
                g_LdqcmbnHandle = handle2;
            }
        } else {
            g_LdqcmbnHandle = handle2;
        }
    }
    free(dir);
    return JNI_TRUE;
}

static jboolean unload(JNIEnv* env, jclass clazz) {
    if (!g_UnityHandle) return JNI_TRUE;
    void* handle = g_UnityHandle;
    g_UnityHandle = NULL;
    JavaVM* vm = NULL;
    if ((*env)->GetJavaVM(env, &vm) != JNI_OK || !vm) {
        (*env)->FatalError(env, "Unable to retrieve Java VM");
        return JNI_TRUE;
    }
    void (*onUnload)(JavaVM*, void*) = (void (*)(JavaVM*, void*))dlsym(handle, "JNI_OnUnload");
    if (onUnload) onUnload(vm, NULL);
    dlclose(handle);
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    JNIEnv* env = NULL;
    if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK || !env) return JNI_ERR;
    jclass clazz = (*env)->FindClass(env, "com/unity3d/player/NativeLoader");
    if (!clazz) {
        (*env)->FatalError(env, "com/unity3d/player/NativeLoader");
        return JNI_ERR;
    }
    static JNINativeMethod methods[] = {
        {"load", "(Ljava/lang/String;)Z", (void*)load},
        {"unload", "()Z", (void*)unload}
    };
    if ((*env)->RegisterNatives(env, clazz, methods, 2) < 0) {
        (*env)->FatalError(env, "com/unity3d/player/NativeLoader");
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
