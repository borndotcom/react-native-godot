/**************************************************************************/
/*  GodotModule.mm                                                        */
/**************************************************************************/
/* Copyright (c) 2024-2025 Slay GmbH                                      */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#import "GodotModule.h"

#define LOG_TAG "GodotModule"
#include "godot-log.h"

#include <libgodot/libgodot.h>
#include <godot_cpp/classes/display_server_embedded.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/gd_extension_manager.hpp>
#include <godot_cpp/classes/rendering_native_surface.hpp>
#include <godot_cpp/classes/rendering_native_surface_apple.hpp>
#include <godot_cpp/godot.hpp>

#include <dlfcn.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <string>

static constexpr NSInteger kGodotMaxFramesPerSecond = 60;

static NSInteger getGodotPreferredFramesPerSecond() {
	godot::Engine *engine = godot::Engine::get_singleton();
	if (!engine) {
		return kGodotMaxFramesPerSecond;
	}

	const int32_t configuredMaxFps = engine->get_max_fps();
	if (configuredMaxFps <= 0) {
		return kGodotMaxFramesPerSecond;
	}

	const NSInteger requestedFramesPerSecond = static_cast<NSInteger>(configuredMaxFps);
	return std::min(requestedFramesPerSecond, kGodotMaxFramesPerSecond);
}

static void configureGodotDisplayLink(CADisplayLink *displayLink) {
	const NSInteger preferredFramesPerSecond = getGodotPreferredFramesPerSecond();
	if (@available(iOS 15.0, *)) {
		// Apple gives 30 Hz and 60 Hz special priority for games. Allowing the
		// system to fall back to 30 Hz also handles thermal and power policies
		// more gracefully than demanding an exact 60 Hz cadence.
		const NSInteger minimumFramesPerSecond =
				std::min<NSInteger>(30, preferredFramesPerSecond);
		displayLink.preferredFrameRateRange = CAFrameRateRangeMake(
				minimumFramesPerSecond,
				preferredFramesPerSecond,
				preferredFramesPerSecond);
	} else {
		displayLink.preferredFramesPerSecond = preferredFramesPerSecond;
	}
}

@interface GodotThread : NSObject

// Method to start the thread and run loop
- (void)start;

// Method to submit blocks to the background thread for execution
- (void)performBlock:(void (^)(void))block waitUntilDone:(bool)wait;

- (void)scheduleBlock:(void (^)(void))block;

- (void)step:(CADisplayLink *)sender;

@end

@interface GodotThread ()

// Strong reference to the thread
@property(nonatomic, strong) NSThread *thread;

// Run loop source and port
@property(nonatomic, strong) NSPort *runLoopPort;

@end

@implementation GodotThread

// Method to start the background thread
- (void)start {
	self.thread = [[NSThread alloc] initWithTarget:self selector:@selector(threadEntryPoint) object:nil];
	self.thread.qualityOfService = NSQualityOfServiceUserInteractive;
	[self.thread start];
}

// Entry point of the background thread
- (void)threadEntryPoint {
	@autoreleasepool {
		// Set up the run loop
		self.runLoopPort = [NSPort port];
		[[NSRunLoop currentRunLoop] addPort:self.runLoopPort forMode:NSDefaultRunLoopMode];

		// Keep the run loop running
		[[NSRunLoop currentRunLoop] run];
	}
}

// Method to perform a block on the background thread
- (void)performBlock:(void (^)(void))block waitUntilDone:(bool)wait {
	if (!block) {
		return;
	}

	// Ensure the block is executed on the background thread
	if ([[NSThread currentThread] isEqual:self.thread]) {
		block();
	} else {
		[self performSelector:@selector(executeBlock:) onThread:self.thread withObject:[block copy] waitUntilDone:wait];
	}
}

- (void)scheduleBlock:(void (^)(void))block {
	if (!block) {
		return;
	}

	[self performSelector:@selector(executeBlock:) onThread:self.thread withObject:[block copy] waitUntilDone:FALSE];
}

// Helper method to execute the block on the background thread
- (void)executeBlock:(void (^)(void))block {
	block();
}

- (void)step:(CADisplayLink *)sender {
	GodotModule::get_singleton()->iterate();
}

@end

typedef GDExtensionObjectPtr (*libgodot_create_godot_instance_type)(int, char *[], GDExtensionInitializationFunction, InvokeCallbackFunction, ExecutorData, InvokeCallbackFunction, ExecutorData, LogCallbackFunction, LogCallbackData);
typedef void (*libgodot_destroy_godot_instance_type)(GDExtensionObjectPtr p_godot_instance);

struct ApplePlatformData : PlatformData {
	CALayer *mainWindowLayer;
	godot::Ref<godot::RenderingNativeSurface> mainSurface;
	CGFloat contentScaleFactor = 1.0;
	CADisplayLink *displayLink = nullptr;
	bool in_background = false;
	bool paused = false;
	void *handle = nullptr;
	libgodot_create_godot_instance_type func_libgodot_create_godot_instance = nullptr;
	libgodot_destroy_godot_instance_type func_libgodot_destroy_godot_instance = nullptr;

	std::map<std::string, std::function<void(bool)>> windowUpdateCallbacks;
	std::map<void *, std::string> handleToWindowName;
	std::mutex windowUpdateMutex;
	std::mutex createMutex;
	GodotThread *thread = nil;

	ApplePlatformData() {
		thread = [[GodotThread alloc] init];
		[thread start];
	}
};

GodotModule *GodotModule::get_singleton() {
	static GodotModule *singleton = new GodotModule(new ApplePlatformData());
	return singleton;
}

extern "C" {

static void initialize_default_module(godot::ModuleInitializationLevel p_level) {
	if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

static void uninitialize_default_module(godot::ModuleInitializationLevel p_level) {
	if (p_level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

static void logCallBack(LogCallbackData p_data, const char *p_log_message, bool p_err) {
	GodotModule *gm = (GodotModule *)p_data;
	gm->log(p_log_message, p_err);
}

GDExtensionBool GDE_EXPORT gdextension_default_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_object(p_get_proc_address, p_library, r_initialization);

	init_object.register_initializer(initialize_default_module);
	init_object.register_terminator(uninitialize_default_module);
	init_object.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_object.init();
}
}

godot::GodotInstance *GodotModule::get_or_create_instance(std::vector<std::string> args) {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);

	// Make sure that we only run this method once at a time.
	std::lock_guard createLock(data->createMutex);

	{
		std::lock_guard lock(_mutex);
		if (_instance) {
			return _instance;
		}
	}

	void *handle = nullptr;
	if (data->func_libgodot_create_godot_instance == nullptr) {
		handle = dlopen("libgodot.framework/libgodot", RTLD_LAZY | RTLD_LOCAL | RTLD_FIRST);

		if (handle == nullptr) {
			NSLog(@"Unable to open libgodot.framework: %s", dlerror());
			return nullptr;
		}
		libgodot_create_godot_instance_type func_libgodot_create_godot_instance = (libgodot_create_godot_instance_type)dlsym(handle, "libgodot_create_godot_instance");

		if (func_libgodot_create_godot_instance == nullptr) {
			NSLog(@"Unable to load libgodot_create_godot_instance symbol: %s", dlerror());
			dlclose(handle);
			return nullptr;
		}

		{
			std::lock_guard lock(_mutex);
			data->func_libgodot_create_godot_instance = func_libgodot_create_godot_instance;
		}
	}

	godot::GodotInstance *instance = nullptr;

	std::vector<std::string> cmdline{ [[[NSBundle mainBundle] executablePath] UTF8String] };
	for (const std::string &arg : args) {
		cmdline.push_back(arg);
	}

	std::vector<const char *> cargs{};
	for (const std::string &arg : cmdline) {
		cargs.push_back(arg.c_str());
	}

	GDExtensionObjectPtr instance_ptr = data->func_libgodot_create_godot_instance(cargs.size(), (char **)cargs.data(), gdextension_default_init, nullptr, nullptr, nullptr, nullptr, logCallBack, this); // TODO: Do we want to use the extra 4 params?
	if (instance_ptr == nullptr) {
		// Unable to start Godot
		NSLog(@"Unable to start Godot");
		if (handle) {
			dlclose(handle);
			handle = nullptr;
			{
				std::lock_guard lock(_mutex);
				data->func_libgodot_create_godot_instance = nullptr;
			}
		}
		return nullptr;
	}

	instance = reinterpret_cast<godot::GodotInstance *>(godot::internal::get_object_instance_binding(instance_ptr));

	CGRect screen = [[UIScreen mainScreen] bounds];
	CGFloat contentScaleFactor = [[UIScreen mainScreen] scale];

	LOGI("Initialize Main Window Layer on the main thread");

	// Godot automatically creates the correct type of layer based on the selected rendering driver
	godot::Ref<godot::RenderingNativeSurfaceApple> appleSurface = godot::RenderingNativeSurfaceApple::create(0);
	CALayer __block *mainWindowLayer = (__bridge CALayer *)(void *)appleSurface->get_layer();
	mainWindowLayer.bounds = CGRectMake(0, 0, screen.size.width, screen.size.height);
	mainWindowLayer.position = CGPointMake(0, 0);
	mainWindowLayer.anchorPoint = CGPointMake(0, 0);
	mainWindowLayer.contentsScale = contentScaleFactor;

	godot::RenderingNativeSurface *ptr = godot::Object::cast_to<godot::RenderingNativeSurface>(appleSurface.ptr());
	godot::Ref<godot::RenderingNativeSurface> nativeSurface(ptr);

	godot::DisplayServerEmbedded::set_native_surface(nativeSurface);

	instance->start();

	{
		std::lock_guard lock(_mutex);

		data->displayLink = [CADisplayLink displayLinkWithTarget:data->thread
														selector:@selector(step:)];
		configureGodotDisplayLink(data->displayLink);
		[data->displayLink addToRunLoop:[NSRunLoop currentRunLoop]
								forMode:NSRunLoopCommonModes];
		data->mainWindowLayer = mainWindowLayer;
		data->mainSurface = nativeSurface;
		data->contentScaleFactor = contentScaleFactor;
		data->handle = handle;

		_instance = instance;
	}

	updateWindows(true);

	return instance;
}

double GodotModule::get_content_scale_factor() {
	std::lock_guard lock(_mutex);
	if (!_instance) {
		return 1.0;
	}

	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);

	return data->contentScaleFactor;
}

void GodotModule::destroy_instance() {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);

	// Make sure that we only run this method once at a time.
	std::lock_guard createLock(data->createMutex);

	if (!_instance) {
		NSLog(@"Godot instance is already destroyed.");
		return;
	}

	if (!data->func_libgodot_destroy_godot_instance) {
		libgodot_destroy_godot_instance_type func_libgodot_destroy_godot_instance = nullptr;
		void *handle = nullptr;

		{
			std::lock_guard lock(_mutex);
			handle = data->handle;
		}

		if (handle == nullptr) {
			NSLog(@"Unable to open libgodot.framework: %s", dlerror());
			return;
		}
		func_libgodot_destroy_godot_instance = (libgodot_destroy_godot_instance_type)dlsym(handle, "libgodot_destroy_godot_instance");

		if (func_libgodot_destroy_godot_instance == nullptr) {
			NSLog(@"Unable to load libgodot_destroy_godot_instance symbol: %s", dlerror());
			dlclose(handle);
			handle = nullptr;
			return;
		}

		{
			std::lock_guard lock(_mutex);
			data->func_libgodot_destroy_godot_instance = func_libgodot_destroy_godot_instance;
		}
	}

	{
		std::lock_guard lock(_mutex);

		[data->displayLink removeFromRunLoop:[NSRunLoop currentRunLoop] forMode:NSRunLoopCommonModes];
		data->displayLink = nil;

		updateWindows(false);

		godot::DisplayServerEmbedded::set_native_surface(godot::Ref<godot::RenderingNativeSurface>(nullptr));
		data->mainWindowLayer = nil;
		data->mainSurface = godot::Ref<godot::RenderingNativeSurface>(nullptr);

		data->func_libgodot_destroy_godot_instance(_instance->_owner);
		_instance = nullptr;
		godot::GDExtensionBinding::deinit();

		dlclose(data->handle);
		data->handle = nullptr;
	}
}

godot::Ref<godot::RenderingNativeSurface> GodotModule::get_main_rendering_surface() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	return data->mainSurface;
}

void *GodotModule::get_main_rendering_layer() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	return (__bridge void *)data->mainWindowLayer;
}

void GodotModule::focus_out() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	data->in_background = true;
	[data->thread scheduleBlock:^{
		std::lock_guard lock(_mutex);
		if (_instance) {
			_instance->focus_out();
		}
	}];
	updateState();
}

void GodotModule::focus_in() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	data->in_background = false;
	[data->thread scheduleBlock:^{
		std::lock_guard lock(_mutex);
		if (_instance) {
			_instance->focus_in();
		}
	}];
	updateState();
}

bool GodotModule::is_paused() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	return data->paused;
}

void GodotModule::appPause() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	UIBackgroundTaskIdentifier bgTask = [[UIApplication sharedApplication] beginBackgroundTaskWithExpirationHandler:^{

	}];
	data->in_background = true;
	[data->thread scheduleBlock:^{
		std::lock_guard lock(_mutex);
		if (_instance) {
			_instance->pause();
		}
		[[UIApplication sharedApplication] endBackgroundTask:bgTask];
	}];
	updateState();
}

void GodotModule::appResume() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	data->in_background = false;
	[data->thread scheduleBlock:^{
		std::lock_guard lock(_mutex);
		if (_instance) {
			_instance->resume();
		}
	}];
	updateState();
}

void GodotModule::pause() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	data->paused = true;
	updateState();
}

void GodotModule::resume() {
	std::lock_guard lock(_mutex);
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	data->paused = false;
	updateState();
}

void GodotModule::updateState() {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	if (!_instance) {
		return;
	}
	if (data->in_background || data->paused) {
		if (data->displayLink) {
			data->displayLink.paused = true;
			[data->displayLink invalidate];
			data->displayLink = nil;
		}
	} else {
		if (!data->displayLink) {
			[data->thread scheduleBlock:^{
				std::lock_guard lock(_mutex);
				ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
				if (!_instance) {
					return;
				}
				if (!data->displayLink) {
					data->displayLink = [CADisplayLink displayLinkWithTarget:data->thread
																	selector:@selector(step:)];
					configureGodotDisplayLink(data->displayLink);
					[data->displayLink addToRunLoop:[NSRunLoop currentRunLoop]
											forMode:NSRunLoopCommonModes];
				}
			}];
		}
	}
}

class CPPCallable : public godot::CallableCustom {
	//    friend bool javascript_callable_compare_equal_func(const godot::CallableCustom *p_a, const godot::CallableCustom *p_b) {
	//        if (p_a == p_b) return true;
	//        const JavascriptCallable *a = static_cast<const JavascriptCallable *>(p_a);
	//        const JavascriptCallable *b = static_cast<const JavascriptCallable *>(p_b);
	//        return jsi::Function::strictEquals(a->_rt, a->_func, b->_func);
	//    };

	std::function<void(const godot::Variant **, int, godot::Variant &, GDExtensionCallError &)> _func;

public:
	CPPCallable(std::function<void(const godot::Variant **, int, godot::Variant &, GDExtensionCallError &)> f) :
			_func(f) {}

	uint32_t hash() const override {
		return 0; // Use default hash function
	}
	godot::String get_as_text() const override {
		return godot::String("CPPCallable");
	}
	CompareEqualFunc get_compare_equal_func() const override {
		return nullptr;
	}

	CompareLessFunc get_compare_less_func() const override {
		return nullptr;
	}

	bool is_valid() const override {
		return true;
	}
	godot::ObjectID get_object() const override {
		return godot::ObjectID();
	}
	void call(const godot::Variant **p_arguments, int p_argcount, godot::Variant &r_return_value, GDExtensionCallError &r_call_error) const override {
		_func(p_arguments, p_argcount, r_return_value, r_call_error);
		r_call_error.error = GDEXTENSION_CALL_OK;
	}
};

godot::Callable GodotModule::create_callable(std::function<void(const godot::Variant **, int, godot::Variant &, GDExtensionCallError &)> f) {
	return godot::Callable(memnew(CPPCallable(f)));
}

void GodotModule::registerWindowUpdateCallback(std::string name, void *handle, std::function<void(bool)> f, void *ref) {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	std::lock_guard lock(data->windowUpdateMutex);
	NSLog(@"Registering Window: %llx, %s", (uint64_t)handle, name.c_str());
	if (data->handleToWindowName.contains(handle)) {
		std::string currentName = data->handleToWindowName[handle];
		if (currentName != name) {
			NSLog(@"ERROR: registerWindowUpdateCallback: Unable to register a different name for the same handle");
			return;
		}
	} else {
		data->handleToWindowName[handle] = name;
	}
	data->windowUpdateCallbacks[name] = f;
}

void GodotModule::unregisterWindowUpdateCallback(void *handle) {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	std::lock_guard lock(data->windowUpdateMutex);
	if (data->handleToWindowName.contains(handle)) {
		std::string name = data->handleToWindowName[handle];
		NSLog(@"Unregistering Window: %llx, %s", (uint64_t)handle, name.c_str());
		data->windowUpdateCallbacks.erase(name);
		data->handleToWindowName.erase(handle);
	}
}

void GodotModule::updateWindow(std::string name, bool adding) {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	std::function<void(bool)> cb;
	{
		std::lock_guard lock(data->windowUpdateMutex);
		if (!data->windowUpdateCallbacks.contains(name)) {
			return;
		}
		cb = data->windowUpdateCallbacks[name];
	}
	cb(adding);
}

void GodotModule::updateWindows(bool adding) {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	std::map<std::string, std::function<void(bool)>> callbacks;
	{
		std::lock_guard lock(data->windowUpdateMutex);
		callbacks = data->windowUpdateCallbacks;
	}
	for (const std::pair<std::string, std::function<void(bool)>> elem : callbacks) {
		NSLog(@"Updating Window: %s", elem.first.c_str());
		elem.second(adding);
	}
}

void GodotModule::runOnGodotThread(std::function<void()> f, bool wait) {
	ApplePlatformData *data = static_cast<ApplePlatformData *>(_data);
	[data->thread
			 performBlock:^{
				 f();
			 }
			waitUntilDone:wait];
}

void GodotModule::iterate() {
	godot::GodotInstance *instance = nullptr;
	{
		std::lock_guard lock(_mutex);
		instance = _instance;
	}
	if (instance && instance->is_started()) {
		instance->iteration();
	}
}
