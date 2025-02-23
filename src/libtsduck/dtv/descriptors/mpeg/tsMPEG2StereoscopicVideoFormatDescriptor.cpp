//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------

#include "tsMPEG2StereoscopicVideoFormatDescriptor.h"
#include "tsDescriptor.h"
#include "tsTablesDisplay.h"
#include "tsPSIRepository.h"
#include "tsPSIBuffer.h"
#include "tsDuckContext.h"
#include "tsxmlElement.h"

#define MY_XML_NAME u"MPEG2_stereoscopic_video_format_descriptor"
#define MY_CLASS    ts::MPEG2StereoscopicVideoFormatDescriptor
#define MY_EDID     ts::EDID::Regular(ts::DID_MPEG_STEREO_VIDEO_FORMAT, ts::Standards::MPEG)

TS_REGISTER_DESCRIPTOR(MY_CLASS, MY_EDID, MY_XML_NAME, MY_CLASS::DisplayDescriptor);


//----------------------------------------------------------------------------
// Constructors
//----------------------------------------------------------------------------

ts::MPEG2StereoscopicVideoFormatDescriptor::MPEG2StereoscopicVideoFormatDescriptor() :
    AbstractDescriptor(MY_EDID, MY_XML_NAME)
{
}

void ts::MPEG2StereoscopicVideoFormatDescriptor::clearContent()
{
    arrangement_type.reset();
}

ts::MPEG2StereoscopicVideoFormatDescriptor::MPEG2StereoscopicVideoFormatDescriptor(DuckContext& duck, const Descriptor& desc) :
    MPEG2StereoscopicVideoFormatDescriptor()
{
    deserialize(duck, desc);
}


//----------------------------------------------------------------------------
// Serialization
//----------------------------------------------------------------------------

void ts::MPEG2StereoscopicVideoFormatDescriptor::serializePayload(PSIBuffer& buf) const
{
    buf.putBit(arrangement_type.has_value());
    buf.putBits(arrangement_type.has_value() ? arrangement_type.value() : 0xFF, 7);
}

void ts::MPEG2StereoscopicVideoFormatDescriptor::deserializePayload(PSIBuffer& buf)
{
    if (buf.getBool()) {
        buf.getBits(arrangement_type, 7);
    }
    else {
        buf.skipBits(7);
    }
}


//----------------------------------------------------------------------------
// Static method to display a descriptor.
//----------------------------------------------------------------------------

void ts::MPEG2StereoscopicVideoFormatDescriptor::DisplayDescriptor(TablesDisplay& disp, const ts::Descriptor& desc, PSIBuffer& buf, const UString& margin, const ts::DescriptorContext& context)
{
    if (buf.canReadBytes(1)) {
        if (buf.getBool()) {
            disp << margin << UString::Format(u"Arrangement type: %n", buf.getBits<uint8_t>(7)) << std::endl;
        }
        else {
            buf.skipBits(7);
        }
    }
}


//----------------------------------------------------------------------------
// XML serialization
//----------------------------------------------------------------------------

void ts::MPEG2StereoscopicVideoFormatDescriptor::buildXML(DuckContext& duck, xml::Element* root) const
{
    root->setOptionalIntAttribute(u"arrangement_type", arrangement_type, true);
}

bool ts::MPEG2StereoscopicVideoFormatDescriptor::analyzeXML(DuckContext& duck, const xml::Element* element)
{
    return element->getOptionalIntAttribute(arrangement_type, u"arrangement_type", 0x00, 0x7F);
}
