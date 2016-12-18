/*
* Copyright 2011-2016 Branimir Karadzic. All rights reserved.
* License: http://www.opensource.org/licenses/BSD-2-Clause
*/

#include "hmd_openvr.h"

#if BGFX_CONFIG_USE_OPENVR

#include <memory>

#	if BGFX_CONFIG_RENDERER_DIRECT3D11
#		include "renderer_d3d11.h"
#	endif // BGFX_CONFIG_RENDERER_DIRECT3D11

namespace bgfx
{
	static void openvrTransformToQuat(float* quat, const float mat34[3][4])
	{
		const float trace = mat34[0][0] + mat34[1][1] + mat34[2][2];

		if (trace > 0.0f)
		{
			const float s = 2.0f * sqrtf(1.0f + trace);
			quat[0] = (mat34[2][1] - mat34[1][2]) / s;
			quat[1] = (mat34[0][2] - mat34[2][0]) / s;
			quat[2] = (mat34[1][0] - mat34[0][1]) / s;
			quat[3] = s * 0.25f;
		}
		else if ((mat34[0][0] > mat34[1][1]) && (mat34[0][0] > mat34[2][2]))
		{
			const float s = 2.0f * sqrtf(1.0f + mat34[0][0] - mat34[1][1] - mat34[2][2]);
			quat[0] = s * 0.25f;
			quat[1] = (mat34[0][1] + mat34[1][0]) / s;
			quat[2] = (mat34[2][0] + mat34[0][2]) / s;
			quat[3] = (mat34[2][1] - mat34[1][2]) / s;
		}
		else if (mat34[1][1] > mat34[2][2])
		{
			const float s = 2.0f * sqrtf(1.0f + mat34[1][1] - mat34[0][0] - mat34[2][2]);
			quat[0] = (mat34[0][1] + mat34[1][0]) / s;
			quat[1] = s * 0.25f;
			quat[2] = (mat34[1][2] + mat34[2][1]) / s;
			quat[3] = (mat34[0][2] - mat34[2][0]) / s;
		}
		else
		{
			const float s = 2.0f * sqrtf(1.0f + mat34[2][2] - mat34[0][0] - mat34[1][1]);
			quat[0] = (mat34[0][2] + mat34[2][0]) / s;
			quat[1] = (mat34[1][2] + mat34[2][1]) / s;
			quat[2] = s * 0.25f;
			quat[3] = (mat34[1][0] - mat34[0][1]) / s;
		}
	}

	VRImplOpenVR::VRImplOpenVR()
	{
	}

	VRImplOpenVR::~VRImplOpenVR()
	{
	}

	bool VRImplOpenVR::init()
	{
		return vr::VR_IsRuntimeInstalled();
	}

	void VRImplOpenVR::shutdown()
	{
	}

	void VRImplOpenVR::connect(VRDesc* desc)
	{
		vr::EVRInitError err;
		m_system = vr::VR_Init(&err, vr::VRApplication_Scene);
		if (err != vr::VRInitError_None)
		{
			vr::VR_Shutdown();
			BX_TRACE("Failed to initialize OpenVR: %d", err);
			return;
		}

		// locate the adapter LUID
		int32_t adapterIndex;
		m_system->GetDXGIOutputInfo(&adapterIndex);
		if (adapterIndex == -1)
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			BX_TRACE("Failed to query the adapter index for OpenVR");
			return;
		}

		struct FreeLib
		{
			void operator()(void* m) { if (m) bx::dlclose(m); }
		};
		std::unique_ptr<void, FreeLib> dxgiLib(bx::dlopen("dxgi.dll"));
		if (!dxgiLib)
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			BX_TRACE("Failed to open dxgi.dll");
			return;
		}

		typedef HRESULT(WINAPI *CreateDXGIFactory1Fn)(REFIID riid, _Out_ void **ppFactory);
		CreateDXGIFactory1Fn CreateDXGIFactory1 = static_cast<CreateDXGIFactory1Fn>(bx::dlsym(dxgiLib.get(), "CreateDXGIFactory1"));
		if (!CreateDXGIFactory1)
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			BX_TRACE("Failed to locate CreateDXGIFactory1 function in dxgi.dll");
			return;
		}

		IDXGIFactory* factory;
		HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory));
		if (FAILED(hr))
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			BX_TRACE("Failed to create DXGI factory: %08X", hr);
			return;
		}

		IDXGIAdapter* adapter;
		hr = factory->EnumAdapters(adapterIndex, &adapter);
		factory->Release();
		if (FAILED(hr))
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			BX_TRACE("Failed to enumerate DXGI adapter: %08X", hr);
			return;
		}

		DXGI_ADAPTER_DESC adapterDesc;
		hr = adapter->GetDesc(&adapterDesc);
		adapter->Release();
		if (FAILED(hr))
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			BX_TRACE("Failed to query DXGI adapter descrption: %08X", hr);
			return;
		}

		// get the compositor
		m_compositor = static_cast<vr::IVRCompositor*>(vr::VR_GetGenericInterface(vr::IVRCompositor_Version, &err));
		if (!m_compositor)
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			BX_TRACE("Failed to obtain compositor interface: %d", err);
			return;
		}

		vr::IVRExtendedDisplay* extdisp = static_cast<vr::IVRExtendedDisplay*>(vr::VR_GetGenericInterface(vr::IVRExtendedDisplay_Version, &err));
		if (!extdisp)
		{
			vr::VR_Shutdown();
			m_system = nullptr;
			m_compositor = nullptr;
			BX_TRACE("Failed to obtain extended display interface: %d", err);
			return;
		}

		int32_t x, y;
		extdisp->GetWindowBounds(&x, &y, &desc->m_deviceSize.m_w, &desc->m_deviceSize.m_h);

		m_system->GetRecommendedRenderTargetSize(&desc->m_eyeSize[0].m_w, &desc->m_eyeSize[0].m_h);
		desc->m_eyeSize[1] = desc->m_eyeSize[0];

		desc->m_deviceType = 1; // TODO: Set this to something reasonable
		desc->m_refreshRate = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float);

		vr::ETrackedPropertyError propErr;
		desc->m_neckOffset[0] = m_system->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserHeadToEyeDepthMeters_Float, &propErr);
		if (desc->m_neckOffset[0] == 0.0f)
		{
			desc->m_neckOffset[0] = 0.0805f;
		}
		desc->m_neckOffset[1] = 0.075f;

		for (int eye = 0; eye != 2; ++eye)
		{
			m_system->GetProjectionRaw(static_cast<vr::EVREye>(eye), &desc->m_eyeFov[eye].m_left, &desc->m_eyeFov[eye].m_right, &desc->m_eyeFov[eye].m_down, &desc->m_eyeFov[eye].m_up);
			desc->m_eyeFov[eye].m_left *= -1.0f;
			desc->m_eyeFov[eye].m_down *= -1.0f;

			auto xform = m_system->GetEyeToHeadTransform(static_cast<vr::EVREye>(eye));
			m_eyeOffsets[eye].offset[0] = xform.m[0][3];
			m_eyeOffsets[eye].offset[1] = xform.m[1][3];
			m_eyeOffsets[eye].offset[2] = xform.m[2][3];
		}
	}

	void VRImplOpenVR::disconnect()
	{
		vr::VR_Shutdown();
		m_system = nullptr;
		m_compositor = nullptr;
		m_leftControllerId = m_rightControllerId = vr::k_unTrackedDeviceIndexInvalid;
	}

	bool VRImplOpenVR::updateTracking(HMD& hmd)
	{
		vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
		auto err = m_compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, nullptr, 0);
		if (err != vr::VRCompositorError_None)
		{
			return false;
		}

		const auto& headPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
		if (headPose.bPoseIsValid)
		{
			if (headPose.eTrackingResult == vr::TrackingResult_Running_OK)
			{
				// convert to position/quat
				hmd.eye[0].translation[0] = headPose.mDeviceToAbsoluteTracking.m[0][3];
				hmd.headTracking.position[1] = headPose.mDeviceToAbsoluteTracking.m[1][3];
				hmd.headTracking.position[2] = headPose.mDeviceToAbsoluteTracking.m[2][3];
				openvrTransformToQuat(hmd.headTracking.rotation, headPose.mDeviceToAbsoluteTracking.m);

				// calculate eye translations in tracked space
				for (int eye = 0; eye != 2; ++eye)
				{
					const float uvx = 2.0f * (hmd.headTracking.rotation[1] * m_eyeOffsets[eye].offset[2] - hmd.headTracking.rotation[2] * m_eyeOffsets[eye].offset[1]);
					const float uvy = 2.0f * (hmd.headTracking.rotation[2] * m_eyeOffsets[eye].offset[0] - hmd.headTracking.rotation[0] * m_eyeOffsets[eye].offset[2]);
					const float uvz = 2.0f * (hmd.headTracking.rotation[0] * m_eyeOffsets[eye].offset[1] - hmd.headTracking.rotation[1] * m_eyeOffsets[eye].offset[0]);
					hmd.eye[eye].translation[0] = m_eyeOffsets[eye].offset[0] + hmd.headTracking.rotation[3] * uvx + hmd.headTracking.rotation[1] * uvz - hmd.headTracking.rotation[2] * uvy + hmd.headTracking.position[0];
					hmd.eye[eye].translation[1] = m_eyeOffsets[eye].offset[1] + hmd.headTracking.rotation[3] * uvy + hmd.headTracking.rotation[2] * uvx - hmd.headTracking.rotation[0] * uvz + hmd.headTracking.position[1];
					hmd.eye[eye].translation[2] = m_eyeOffsets[eye].offset[2] + hmd.headTracking.rotation[3] * uvz + hmd.headTracking.rotation[0] * uvy - hmd.headTracking.rotation[1] * uvx + hmd.headTracking.position[2];
				}
			}
		}

		return true;
	}

	void VRImplOpenVR::recenter()
	{
		m_system->ResetSeatedZeroPose();
	}

	void VRImplOpenVR::updateInput(HMD& /* _hmd */)
	{
	}
}

#endif // BGFX_CONFIG_USE_OPENVR
