/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "hmd_openvr.h"
#include <bx/fpumath.h>

#if BGFX_CONFIG_USE_OPENVR

BX_PRAGMA_DIAGNOSTIC_IGNORED_CLANG_GCC("-Wunused-variable")

namespace bgfx
{
#if BX_PLATFORM_WINDOWS
#	define VR_CALLTYPE __cdecl
#else
#	define VR_CALLTYPE
#endif

	typedef uint32_t    (VR_CALLTYPE *PFN_VR_INITINTERNAL)(EVRInitError* peError, EVRApplicationType eType);
	typedef void        (VR_CALLTYPE *PFN_VR_SHUTDOWNINTERNAL)();
	typedef bool        (VR_CALLTYPE *PFN_VR_ISHMDPRESENT)();
	typedef void*       (VR_CALLTYPE *PFN_VR_GETGENERICINTERFACE)(const char* pchInterfaceVersion, EVRInitError* peError);
	typedef bool        (VR_CALLTYPE *PFN_VR_ISRUNTIMEINSTALLED)();
	typedef bool        (VR_CALLTYPE *PFN_VR_ISINTERFACEVERSIONVALID)(const char *pchInterfaceVersion);
	typedef uint32_t    (VR_CALLTYPE *PFN_VR_GETINITTOKEN)();
	typedef const char* (VR_CALLTYPE *PFN_VR_GETVRINITERRORASSYMBOL)(EVRInitError error);
	typedef const char* (VR_CALLTYPE *PFN_VR_GETVRINITERRORASENGLISHDESCRIPTION)(EVRInitError error);

	PFN_VR_INITINTERNAL                       VR_InitInternal;
	PFN_VR_SHUTDOWNINTERNAL                   VR_ShutdownInternal;
	PFN_VR_ISHMDPRESENT                       VR_IsHmdPresent;
	PFN_VR_GETGENERICINTERFACE                VR_GetGenericInterface;
	PFN_VR_ISRUNTIMEINSTALLED                 VR_IsRuntimeInstalled;
	PFN_VR_ISINTERFACEVERSIONVALID            VR_IsInterfaceVersionValid;
	PFN_VR_GETINITTOKEN                       VR_GetInitToken;
	PFN_VR_GETVRINITERRORASSYMBOL             VR_GetVRInitErrorAsSymbol;
	PFN_VR_GETVRINITERRORASENGLISHDESCRIPTION VR_GetVRInitErrorAsEnglishDescription;

	void* loadOpenVRDll()
	{
		void* openvrdll = bx::dlopen(
#if BX_PLATFORM_LINUX
			"libopenvr_api.so"
#elif BX_PLATFORM_OSX
			"libopenvr_api.dylib"
#else
			"openvr_api.dll"
#endif // BX_PLATFORM_*
			);
		if (NULL != openvrdll)
		{
			VR_InitInternal            = (PFN_VR_INITINTERNAL           )bx::dlsym(openvrdll, "VR_InitInternal");
			VR_ShutdownInternal        = (PFN_VR_SHUTDOWNINTERNAL       )bx::dlsym(openvrdll, "VR_ShutdownInternal");
			VR_IsHmdPresent            = (PFN_VR_ISHMDPRESENT           )bx::dlsym(openvrdll, "VR_IsHmdPresent");
			VR_GetGenericInterface     = (PFN_VR_GETGENERICINTERFACE    )bx::dlsym(openvrdll, "VR_GetGenericInterface");
			VR_IsRuntimeInstalled      = (PFN_VR_ISRUNTIMEINSTALLED     )bx::dlsym(openvrdll, "VR_IsRuntimeInstalled");
			VR_IsInterfaceVersionValid = (PFN_VR_ISINTERFACEVERSIONVALID)bx::dlsym(openvrdll, "VR_IsInterfaceVersionValid");
			VR_GetInitToken            = (PFN_VR_GETINITTOKEN           )bx::dlsym(openvrdll, "VR_GetInitToken");
			VR_GetVRInitErrorAsSymbol             = (PFN_VR_GETVRINITERRORASSYMBOL            )bx::dlsym(openvrdll, "VR_GetVRInitErrorAsSymbol");
			VR_GetVRInitErrorAsEnglishDescription = (PFN_VR_GETVRINITERRORASENGLISHDESCRIPTION)bx::dlsym(openvrdll, "VR_GetVRInitErrorAsEnglishDescription");

			if (NULL == VR_InitInternal
			||  NULL == VR_ShutdownInternal
			||  NULL == VR_IsHmdPresent
			||  NULL == VR_GetGenericInterface
			||  NULL == VR_IsRuntimeInstalled
			||  NULL == VR_IsInterfaceVersionValid
			||  NULL == VR_GetInitToken
			||  NULL == VR_GetVRInitErrorAsSymbol
			||  NULL == VR_GetVRInitErrorAsEnglishDescription)
			{
				bx::dlclose(openvrdll);
				return NULL;
			}

			EVRInitError err;
			uint32_t token = VR_InitInternal(&err, EVRApplicationType_VRApplication_Scene);
			BX_UNUSED(token);

			BX_TRACE("OpenVR: HMD is %spresent, Runtime is %sinstalled."
				, VR_IsHmdPresent() ? "" : "not "
				, VR_IsRuntimeInstalled() ? "" : "not "
				);
		}

		return openvrdll;
	}

	void unloadOpenVRDll(void* _openvrdll)
	{
		bx::dlclose(_openvrdll);
	}

	VRImplOpenVR::VRImplOpenVR()
		: m_system(NULL)
		, m_compositor(NULL)
		, m_dll(NULL)
	{
	}

	VRImplOpenVR::~VRImplOpenVR()
	{
	}

	static void *loadOpenVRInterface(const char *iface_version) {
		std::string fntable_iface_version;
		bx::stringPrintf(fntable_iface_version, "FnTable:%s", iface_version);
		if (!VR_IsInterfaceVersionValid(fntable_iface_version.c_str()))
		{
			BX_TRACE("Invalid OpenVR interface %s", fntable_iface_version.c_str());
			return NULL;
		}

		EVRInitError error;
		void *iface = VR_GetGenericInterface(fntable_iface_version.c_str(), &error);
		if (error != EVRInitError_VRInitError_None)
		{
			BX_TRACE("Error retrieving OpenVR %s interface: %s", fntable_iface_version.c_str(), VR_GetVRInitErrorAsEnglishDescription(error));

			return NULL;
		}

		return iface;
	}

	bool VRImplOpenVR::init()
	{
		m_dll = loadOpenVRDll();
		if (!m_dll)
		{
			return false;
		}

		return VR_IsHmdPresent();
	}

	void VRImplOpenVR::shutdown()
	{
		unloadOpenVRDll(m_dll);
		m_dll = NULL;
	}

	void VRImplOpenVR::connect(VRDesc* _desc)
	{
		EVRInitError error;
		if (0 == g_platformData.openvrToken)
		{

			VR_InitInternal(&error, EVRApplicationType_VRApplication_Scene);
			if (error != EVRInitError_VRInitError_None) {
				BX_TRACE("Error initializing OpenVR: %s", VR_GetVRInitErrorAsEnglishDescription(error));
				return;
			}
		}

		m_system = (VR_IVRSystem_FnTable *)loadOpenVRInterface(IVRSystem_Version);
		m_compositor = (VR_IVRCompositor_FnTable *)loadOpenVRInterface(IVRCompositor_Version);

		if (!m_system || !m_compositor) {
			m_system = NULL;
			m_compositor = NULL;
			if (0 == g_platformData.openvrToken)
			{
				VR_ShutdownInternal();
			}

			return;
		}

		uint32_t rt_width, rt_height;
		m_system->GetRecommendedRenderTargetSize(&rt_width, &rt_height);
		ETrackedPropertyError tp_error;
		_desc->m_refreshRate = m_system->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_DisplayFrequency_Float, &tp_error);

		_desc->m_eyeSize[0].m_w = _desc->m_eyeSize[1].m_w = rt_width;
		_desc->m_eyeSize[0].m_h = _desc->m_eyeSize[1].m_h = rt_height;
		_desc->m_deviceType = 0; // FIXME there's no shared type enum
		// FIXME OpenVR does not provide these values, and these will not be quite correct
		// however, they don't appear to be used anywhere
		_desc->m_deviceSize.m_w = _desc->m_eyeSize[0].m_w * 2;
		_desc->m_deviceSize.m_w = _desc->m_eyeSize[0].m_h;

		for (int eye = 0; eye < 2; ++eye)
		{
			_desc->m_eyeFov[eye].m_left = m_system->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_FieldOfViewLeftDegrees_Float, &tp_error);
			_desc->m_eyeFov[eye].m_right = m_system->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_FieldOfViewRightDegrees_Float, &tp_error);
			_desc->m_eyeFov[eye].m_up = m_system->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_FieldOfViewTopDegrees_Float, &tp_error);
			_desc->m_eyeFov[eye].m_down = m_system->GetFloatTrackedDeviceProperty(k_unTrackedDeviceIndex_Hmd, ETrackedDeviceProperty_Prop_FieldOfViewBottomDegrees_Float, &tp_error);
		}

		// FIXME openvr does not provide this (and also doesn't look used anywhere... leftover from OVR DK1?)
		_desc->m_neckOffset[0] = _desc->m_neckOffset[1] = 0.0f;
	}

	void VRImplOpenVR::disconnect()
	{
		if (0 == g_platformData.openvrToken)
		{
			VR_ShutdownInternal();
		}
	}

	static void hmdMat34ToFloatArray(float *out, const HmdMatrix34_t &in)
	{
		for (uint32_t ii = 0; ii < 3; ++ii)
		{
			for (uint32_t jj = 0; jj < 4; ++jj)
			{
				out[4 * ii + jj] = in.m[ii][jj];
			}
		}

		out[12] = 0.0f;
		out[13] = 0.0f;
		out[14] = 0.0f;
		out[15] = 1.0f;
	}

	static void hmdMat44ToFloatArray(float *out, const HmdMatrix44_t &in)
	{
		for (uint32_t ii = 0; ii < 4; ++ii)
		{
			for (uint32_t jj = 0; jj < 4; ++jj)
			{
				out[4 * ii + jj] = in.m[jj][ii];
			}
		}
	}

	bool VRImplOpenVR::updateTracking(HMD& _hmd)
	{
		BX_UNUSED(_hmd);
		if (NULL == m_compositor)
		{
			return false;
		}

		TrackedDevicePose_t poses[k_unMaxTrackedDeviceCount];
		m_compositor->WaitGetPoses(poses, BX_COUNTOF(poses), 0, 0);
		const TrackedDevicePose_t& pose = poses[k_unTrackedDeviceIndex_Hmd];
		if (!pose.bPoseIsValid)
		{
			return false;
		}

		float headPose[16];
		hmdMat34ToFloatArray(headPose, pose.mDeviceToAbsoluteTracking);

		for (int eye = 0; eye < 2; ++eye)
		{
			HMD::Eye& hmdEye = _hmd.eye[eye];

			float eyeToHead[16];
			hmdMat34ToFloatArray(eyeToHead, m_system->GetEyeToHeadTransform((EVREye)eye));
			//float eyePose[16];
			//bx::mtxMul(eyePose, headPose, eyeToHead);

			// This is crappy. bgfx should handle any arbitrary eye transform
			// Assuming they have the same rotation is not going to work for some headsets
			hmdEye.viewOffset[0] = -eyeToHead[3];
			hmdEye.viewOffset[1] = -eyeToHead[7];
			hmdEye.viewOffset[2] = -eyeToHead[11];
			hmdEye.translation[0] = headPose[3];
			hmdEye.translation[1] = headPose[7];
			hmdEye.translation[2] = headPose[11];
			bx::quatMtx(hmdEye.rotation, headPose);

			// FIXME OpenVR doesn't appear to provide this directly
			// but it also doesn't appear to be used
			hmdEye.pixelsPerTanAngle[eye] = 0.0f;

			hmdMat44ToFloatArray(hmdEye.projection, m_system->GetProjectionMatrix((EVREye)eye, 0.1f, 1000.0f, EGraphicsAPIConvention_API_DirectX));
		}

		return true;
	}

	void VRImplOpenVR::updateInput(HMD& /* _hmd */)
	{
	}

	void VRImplOpenVR::recenter()
	{
	}

} // namespace bgfx

#endif // BGFX_CONFIG_USE_OPENVR
