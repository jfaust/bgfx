/*
 * Copyright 2011-2016 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#ifndef BGFX_OPENVR_H_HEADER_GUARD
#define BGFX_OPENVR_H_HEADER_GUARD

#if BGFX_CONFIG_USE_OPENVR

#include "bgfx_p.h"
#include "hmd.h"
#include <openvr/openvr_capi.h>

namespace bgfx
{
	class VRImplOpenVR : public VRImplI
	{
	public:
		VRImplOpenVR();
		virtual ~VRImplOpenVR() = 0;

		virtual bool init() BX_OVERRIDE;
		virtual void shutdown() BX_OVERRIDE;
		virtual void connect(VRDesc* _desc) BX_OVERRIDE;
		virtual void disconnect() BX_OVERRIDE;

		virtual bool isConnected() const BX_OVERRIDE
		{
			return NULL != m_system;
		}

		virtual bool updateTracking(HMD& _hmd) BX_OVERRIDE;
		virtual void updateInput(HMD& _hmd) BX_OVERRIDE;
		virtual void recenter() BX_OVERRIDE;

		virtual bool createSwapChain(const VRDesc& _desc, int _msaaSamples, int _mirrorWidth, int _mirrorHeight) BX_OVERRIDE = 0;
		virtual void destroySwapChain() BX_OVERRIDE = 0;
		virtual void destroyMirror() BX_OVERRIDE = 0;
		virtual void renderEyeStart(const VRDesc& _desc, uint8_t _eye) BX_OVERRIDE = 0;
		virtual bool submitSwapChain(const VRDesc& _desc) BX_OVERRIDE = 0;

	protected:
		VR_IVRSystem_FnTable *m_system;
		VR_IVRCompositor_FnTable *m_compositor;
		void *m_dll;
	};

} // namespace bgfx

#endif // BGFX_CONFIG_USE_OPENVR

#endif // BGFX_OPENVR_H_HEADER_GUARD
