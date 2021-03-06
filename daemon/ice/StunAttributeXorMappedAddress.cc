/**
 * @file
 *
 * This file implements the STUN Attribute XOR Mapped Address class
 */

/******************************************************************************
 * Copyright 2009,2012 Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/

#include <qcc/platform.h>

#include "Status.h"
#include <StunMessage.h>
#include <StunAttributeXorMappedAddress.h>

using namespace qcc;

#define QCC_MODULE "STUN_ATTRIBUTE"


enum IPFamily {
    IPV4 = 0x01,
    IPV6 = 0x02
};

QStatus StunAttributeXorMappedAddress::Parse(const uint8_t*& buf, size_t& bufSize)
{
    QStatus status;
    IPFamily family;
    uint8_t xorAddr[IPAddress::IPv6_SIZE];
    const uint8_t* xorBytes = message.rawMsg + (2 * sizeof(uint16_t));
    size_t addrLen;
    size_t index;

    if (bufSize < MIN_ATTR_SIZE) {
        status = ER_BUFFER_TOO_SMALL;
        QCC_LogError(status, ("Parsing Message Integrity attribute"));
        goto exit;
    }

    ++buf;      // Skip unused octet.
    --bufSize;  // Maintain accounting.

    family = static_cast<IPFamily>(*buf);
    ++buf;
    --bufSize;

    ReadNetToHost(buf, bufSize, port);
    port ^= static_cast<uint16_t>(StunMessage::MAGIC_COOKIE >> 16);

    switch (family) {
    case IPV4:
        addrLen = IPAddress::IPv4_SIZE;
        break;

    case IPV6:
        addrLen = IPAddress::IPv6_SIZE;
        break;

    default:
        status = ER_STUN_INVALID_ADDR_FAMILY;
        QCC_LogError(status, ("Parsing Mapped Address attribute"));
        goto exit;
    }

    for (index = 0; index < addrLen; ++index) {
        xorAddr[index] = buf[index] ^ xorBytes[index];
        QCC_DbgPrintf(("buf[%u] = %02x  ^  xorBytes[%u] = %02x  ==>  xorAddr[%u] = %02x",
                       index, buf[index],
                       index, xorBytes[index],
                       index, xorAddr[index]));
    }

    addr = IPAddress(xorAddr, addrLen);

    buf += addrLen;;
    bufSize -= addrLen;

    status = StunAttribute::Parse(buf, bufSize);

exit:
    return status;
}


QStatus StunAttributeXorMappedAddress::RenderBinary(uint8_t*& buf, size_t& bufSize, ScatterGatherList& sg) const
{
    QStatus status;
    uint16_t xorPort;
    uint8_t xorAddr[IPAddress::IPv6_SIZE];
    uint8_t xorBytes[IPAddress::IPv6_SIZE];
    ScatterGatherList xorSG(sg);
    size_t index;

    status = StunAttribute::RenderBinary(buf, bufSize, sg);
    if (status != ER_OK) {
        goto exit;
    }

    // Fill unused octet with 0.
    WriteHostToNet(buf, bufSize, static_cast<uint8_t>(0), sg);

    switch (addr.Size()) {
    case IPAddress::IPv4_SIZE:
        WriteHostToNet(buf, bufSize, static_cast<uint8_t>(IPV4), sg);
        break;

    case IPAddress::IPv6_SIZE:
        WriteHostToNet(buf, bufSize, static_cast<uint8_t>(IPV6), sg);
        break;

    default:
        status = ER_STUN_INVALID_ADDR_FAMILY;
        QCC_LogError(status, ("Rendering %s", name));
        goto exit;
    }

    status = addr.RenderIPBinary(xorAddr, sizeof(xorAddr));
    if (status != ER_OK) {
        goto exit;
    }

    xorPort  = port ^ static_cast<uint16_t>(StunMessage::MAGIC_COOKIE >> 16);
    WriteHostToNet(buf, bufSize, xorPort, sg);

    xorSG.TrimFromBegining(sizeof(uint32_t));
    xorSG.CopyToBuffer(xorBytes, sizeof(xorBytes));

    for (index = 0; index < addr.Size(); index++) {
        buf[index] = xorAddr[index] ^ xorBytes[index];
        QCC_DbgPrintf(("xorAddr[%u] = %02x  ^  xorBytes[%u] = %02x  =>  buf[%u] = %02x",
                       index, xorAddr[index],
                       index, xorBytes[index],
                       index, buf[index]));
    }

    sg.AddBuffer(&buf[0], addr.Size());
    sg.IncDataSize(addr.Size());

    buf += addr.Size();
    bufSize -= addr.Size();

exit:
    return status;
}

