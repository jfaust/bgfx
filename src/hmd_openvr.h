/*
* Copyright 2011-2016 Branimir Karadzic. All rights reserved.
* License: http://www.opensource.org/licenses/BSD-2-Clause
*/

#ifndef BGFX_HMD_OPENVR_H_HEADER_GUARD
#define BGFX_HMD_OPENVR_H_HEADER_GUARD

#include "bgfx_p.h"

#if BGFX_CONFIG_USE_OPENVR

#include "hmd.h"
#include <openvr.h>

namespace bgfx
{
	class VRImplOpenVR : public VRImplI
	{
	public:
		VRImplOpenVR();
		~VRImplOpenVR();

		virtual bool init() BX_OVERRIDE;
		virtual void shutdown() BX_OVERRIDE;
		virtual void connect(VRDesc* _desc) BX_OVERRIDE;
		virtual void disconnect() BX_OVERRIDE;
		virtual bool isConnected() const BX_OVERRIDE
		{
			return NULL != m_compositor;
		}

		virtual bool updateTracking(HMD& hmd) BX_OVERRIDE;
		virtual void updateInput(HMD& hmd) BX_OVERRIDE;
		virtual void recenter() BX_OVERRIDE;

		virtual bool createSwapChain(const VRDesc& _desc, int _msaaSamples, int _mirrorWidth, int _mirrorHeight) BX_OVERRIDE = 0;
		virtual void destroySwapChain() BX_OVERRIDE = 0;
		virtual void destroyMirror() BX_OVERRIDE = 0;
		virtual void makeRenderTargetActive(const VRDesc& _desc) BX_OVERRIDE = 0;
		virtual bool submitSwapChain(const VRDesc& _desc) BX_OVERRIDE = 0;

	protected:
		struct EyeOffset
		{
			float offset[3];
		};

		vr::IVRCompositor* m_compositor = nullptr;
		vr::IVRSystem* m_system = nullptr;
		EyeOffset m_eyeOffsets[2];
		vr::TrackedDeviceIndex_t m_leftControllerId = vr::k_unTrackedDeviceIndexInvalid;
		vr::TrackedDeviceIndex_t m_rightControllerId = vr::k_unTrackedDeviceIndexInvalid;
	};

} // namespace bgfx

#endif // BGFX_CONFIG_USE_OPENVR
#endif // BGFX_HMD_OPENVR_H_HEADER_GUARD
