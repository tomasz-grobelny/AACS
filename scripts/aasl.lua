-- Distributed under GPLv3 only as specified in repository's root LICENSE file

-- This is a wireshark plugin for decoding pcap dumps created with AAServer's dumpfile option

local frame_types = {
    [0] = "Middle",
    [1] = "First",
    [2] = "Last",
    [3] = "Bulk",
}

local message_types = {
    [0] = "None",
    [1] = "Version Request",
    [2] = "Version Response",
    [3] = "SSL Handshake",
    [4] = "Auth Complete",
    [5] = "Service Discovery Request",
    [6] = "Service Discovery Response",
    [7] = "Channel Open Request",
    [8] = "Channel Open Response",
    [0xb] = "Ping Request",
    [0xc] = "Ping Response",
    [0xd] = "Navigation Focus Request",
    [0xe] = "Navigation Focus Response",
    [0xf] = "Shutdown Request",
    [0x10] = "Shutdown Response",
    [0x11] = "Voice Session Request",
    [0x12] = "Audio Focus Request",
    [0x13] = "Audio Focus Response",
}

local aaserverlog = Proto("aalog","Android Auto Server Log")
local aaserverlog_channel = ProtoField.new("Channel", "aasl.channel", ftypes.UINT8)
local aaserverlog_flags = ProtoField.new("Flags", "aasl.flags", ftypes.UINT8, nil, base.HEX)
local aaserverlog_flag_encryption_type = ProtoField.new("Encryption type", "aasl.flags.encryption_type", ftypes.BOOLEAN, {"Encrypted","Plain"}, 8, 0x08, "Is this encrypted?")
local aaserverlog_flag_message_type = ProtoField.new("Message type", "aasl.flags.message_type", ftypes.BOOLEAN, {"Specific","Control"}, 8, 0x04, "Is this control?")
local aaserverlog_flag_frame_type = ProtoField.uint8("aasl.flags.frame_type", "Frame type", base.DEC, frame_types, 0x0003)
local aaserverlog_direction = ProtoField.new("Direction", "aasl.direction", ftypes.BOOLEAN, {"To Headunit", "From Headunit"}, 1, 0x01, "Direction")
local aaserverlog_reserved = ProtoField.new("Reserved", "aasl.reserved", ftypes.UINT8)
local aaserverlog_control_message_type = ProtoField.uint16("aasl.control_message_type", "Message Type", base.DEC, message_types, nil, "Message Type")

local aaserverlog_request_version = ProtoField.new("Version Request", "aasl.version_request", ftypes.STRING)
local aaserverlog_request_version_major = ProtoField.new("Major", "aasl.version_request.major", ftypes.UINT16)
local aaserverlog_request_version_minor = ProtoField.new("Minor", "aasl.version_request.minor", ftypes.UINT16)

local aaserverlog_response_version = ProtoField.new("Version Response", "aasl.version_response", ftypes.STRING)
local aaserverlog_response_version_major = ProtoField.new("Major", "aasl.version_response.major", ftypes.UINT16)
local aaserverlog_response_version_minor = ProtoField.new("Minor", "aasl.version_response.minor", ftypes.UINT16)
local aaserverlog_response_version_match = ProtoField.new("Match", "aasl.version_response.match", ftypes.UINT16)


aaserverlog.fields = {
    aaserverlog_channel,
    aaserverlog_flags,
    aaserverlog_flag_encryption_type,
    aaserverlog_flag_message_type,
    aaserverlog_flag_frame_type,
    aaserverlog_direction,
    aaserverlog_reserved,
    aaserverlog_control_message_type,

    aaserverlog_request_version,
    aaserverlog_request_version_major,
    aaserverlog_request_version_minor,
    aaserverlog_response_version,
    aaserverlog_response_version_major,
    aaserverlog_response_version_minor,
    aaserverlog_response_version_match,
}

function aaserverlog.dissector(tvbuf,pktinfo,root)
    pktinfo.cols.protocol:set("AndroidAuto")
    local pktlen = tvbuf:reported_length_remaining()
    local tree = root:add(aaserverlog, tvbuf:range(0, pktlen))

    local offset = 0
    tree:add(aaserverlog_channel, tvbuf:range(offset, 1))
    offset = offset + 1
    local flagrange = tvbuf:range(offset,1)
    offset = offset + 1
    local flag_tree = tree:add(aaserverlog_flags, flagrange)
    flag_tree:add(aaserverlog_flag_encryption_type, flagrange)
    flag_tree:add(aaserverlog_flag_message_type, flagrange)
    flag_tree:add(aaserverlog_flag_frame_type, flagrange)
    tree:add(aaserverlog_direction, tvbuf:range(offset, 1))
    offset = offset + 1
    tree:add(aaserverlog_reserved, tvbuf:range(offset, 1))
    offset = offset + 1
    local control_message_type_range = tvbuf:range(offset, 2)
    tree:add(aaserverlog_control_message_type, control_message_type_range)
    offset = offset + 2
    local control_message_type  = control_message_type_range:uint()
    if control_message_type == 1 then
            local versionrange = tvbuf:range(offset,4)
            local version_string = tostring(tvbuf:range(offset,2):uint()) .. "." .. tostring(tvbuf:range(offset+2,2):uint())
            local version_tree = tree:add(aaserverlog_request_version, versionrange, version_string)
            version_tree:add(aaserverlog_request_version_major, tvbuf:range(offset,2))
            version_tree:add(aaserverlog_request_version_minor, tvbuf:range(offset+2,2))
            pktinfo.cols.info:set("Version Request: " .. version_string)
    elseif control_message_type == 2 then
            local versionrange = tvbuf:range(offset,4)
            local matched = "(NOT MATCHED)"
            if tvbuf:range(offset+4,2):uint() == 0 then
                    matched = "(MATCHED)"
            end
            local version_string = tostring(tvbuf:range(offset,2):uint()) .. "." .. tostring(tvbuf:range(offset+2,2):uint()) .. " " .. matched
            local version_tree = tree:add(aaserverlog_response_version, versionrange, version_string)
            version_tree:add(aaserverlog_response_version_major, tvbuf:range(offset,2))
            version_tree:add(aaserverlog_response_version_minor, tvbuf:range(offset+2,2))
            version_tree:add(aaserverlog_response_version_match, tvbuf:range(offset+4,2))
            pktinfo.cols.info:set("Version Response: " .. version_string)
    elseif control_message_type == 3 then
            local dissector = Dissector.get("tls")
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
    elseif control_message_type == 4 then
            local dissector = Dissector.get("protobuf")
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
            pktinfo.cols.info:set("Auth Complete")
    elseif control_message_type == 5 then
            local dissector = Dissector.get("protobuf")
            pktinfo.private["pb_msg_type"] = "message," .. "tag.aas.ServiceDiscoveryRequest"
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
            pktinfo.cols.info:set("Service Discovery Request")
    elseif control_message_type == 6 then
            local dissector = Dissector.get("protobuf")
            pktinfo.private["pb_msg_type"] = "message," .. "tag.aas.ServiceDiscoveryResponse"
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
            pktinfo.cols.info:set("Service Discovery Response")
    elseif control_message_type == 7 then
            local dissector = Dissector.get("protobuf")
            pktinfo.private["pb_msg_type"] = "message," .. "tag.aas.ChannelOpenRequest"
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
            pktinfo.cols.info:set("Channel Open Request")
    elseif control_message_type == 8 then
            local dissector = Dissector.get("protobuf")
            pktinfo.private["pb_msg_type"] = "message," .. "tag.aas.ChannelOpenResponse"
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
            pktinfo.cols.info:set("Channel Open Response")
    elseif control_message_type == 11 then
            local dissector = Dissector.get("protobuf")
            pktinfo.private["pb_msg_type"] = "message," .. "tag.aas.PingRequest"
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
            pktinfo.cols.info:set("Ping Request")
    elseif control_message_type == 12 then
            local dissector = Dissector.get("protobuf")
            pktinfo.private["pb_msg_type"] = "message," .. "tag.aas.PingResponse"
            dissector:call(tvbuf(offset):tvb(), pktinfo, tree)
            pktinfo.cols.info:set("Ping Response")
    end
    return pktlen
end


DissectorTable.get("null.type"):add(0x00, aaserverlog)
