//-----------------------------------------------------
// Name: BNSprite.cpp
// Author: brianuuu
// Date: 12/6/2020
//-----------------------------------------------------

#include "bnsprite.h"

#include <assert.h>
#include <stdlib.h>

#include <algorithm>
#include <cstring>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

//-----------------------------------------------------
// Constructor
//-----------------------------------------------------
BNSprite::BNSprite()
{
    m_loaded = false;
}

//-----------------------------------------------------
// Destructor
//-----------------------------------------------------
BNSprite::~BNSprite()
{
}

//-----------------------------------------------------
// Clear all data
//-----------------------------------------------------
void BNSprite::Clear()
{
    m_loaded = false;
    m_animations.clear();
    m_tilesets.clear();
    m_paletteGroups.clear();
}

//-----------------------------------------------------
// Load a sprite file, return false if fail
//-----------------------------------------------------
bool BNSprite::LoadBN
(
    wstring const& _fileName,
    string& _errorMsg
)
{
    Clear();

    FILE* f;
    _wfopen_s(&f, _fileName.c_str(), L"rb");
    if (!f)
    {
        _errorMsg = "Unable to open file!";
        fclose(f);
        return false;
    }

    // File size
    fseek(f, 0, SEEK_END);
    uint32_t fileSize = ftell(f);

    // Read metadata
    fseek(f, 0x01, SEEK_SET);
    uint32_t metadataOffset = 0x00;
    {
        uint8_t meta0 = ReadByte(f);
        uint8_t meta1 = ReadByte(f);
        if (meta0 == 0x00 && meta1 == 0x01)
        {
            metadataOffset = 0x04;
        }
        else
        {
            std::cout << "Missing metadata, the program will attempt to guess the number of animations!" << endl;
        }
    }

    // Get number of animations
    uint8_t animCount = 0;
    if (metadataOffset == 0x00)
    {
        fseek(f, 0x00, SEEK_SET);
        animCount = ReadInt(f) / 0x04;
        fseek(f, 0x00, SEEK_SET);
    }
    else
    {
        // At 0x03 right now
        animCount = ReadByte(f);
    }

    if (animCount > 0xFF)
    {
        _errorMsg = "Sprite exceeded 255 animations!";
        fclose(f);
        return false;
    }

    // Read animation pointers
    std::cout << "Reading animation pointers..." << endl;
    vector<uint32_t> animPtrs;
    animPtrs.reserve(animCount);
    for (uint8_t i = 0; i < animCount; i++)
    {
        uint32_t ptr = ReadInt(f) + metadataOffset;
        if (ptr < (metadataOffset + animCount * 0x04) || ptr >= fileSize)
        {
            _errorMsg = "Invalid animation pointer at address " + GetAddressString(ftell(f) - 4);
            fclose(f);
            return false;
        }

        animPtrs.push_back(ptr);
    }

    // Read frame data
    std::cout << "Reading frame data..." << endl;
    uint32_t tilesetID = 0;
    vector<uint32_t> tilesetPtrs;
    uint32_t paletteGroupID = 0;
    vector<uint32_t> paletteGroupPtrs;

    m_animations.reserve(0xFF);
    for (auto animPtr : animPtrs)
    {
        fseek(f, animPtr, SEEK_SET);

        Animation animation;
        bool endOfFrame = false;
        while (!endOfFrame)
        {
            if ((uint32_t)ftell(f) >= fileSize)
            {
                _errorMsg = "Reached the end of file while reading frame data!";
                fclose(f);
                return false;
            }

            Frame frame;

            // Read tileset
            {
                uint32_t ptr = ReadInt(f) + metadataOffset;
                if (ptr <= (uint32_t)ftell(f) - 0x04 || ptr >= fileSize)
                {
                    _errorMsg = "Invalid tileset pointer at address " + GetAddressString(ftell(f) - 4);
                    fclose(f);
                    return false;
                }

                // This can have duplicated pointers
                auto it = find(tilesetPtrs.begin(), tilesetPtrs.end(), ptr);
                if (it != tilesetPtrs.end())
                {
                    // Use existing ID
                    frame.m_tilesetID = it - tilesetPtrs.begin();
                }
                else
                {
                    // Create new ID
                    tilesetPtrs.push_back(ptr);
                    frame.m_tilesetID = tilesetID;
                    tilesetID++;
                }
            }

            // Read palette
            {
                uint32_t ptr = ReadInt(f) + metadataOffset;
                if (ptr <= (uint32_t)ftell(f) - 0x04 || ptr >= fileSize)
                {
                    _errorMsg = "Invalid palette group pointer at address " + GetAddressString(ftell(f) - 4);
                    fclose(f);
                    return false;
                }

                // This can have duplicated pointers
                auto it = find(paletteGroupPtrs.begin(), paletteGroupPtrs.end(), ptr);
                if (it != paletteGroupPtrs.end())
                {
                    // Use existing ID
                    frame.m_paletteGroupID = it - paletteGroupPtrs.begin();
                }
                else
                {
                    // Create new ID
                    paletteGroupPtrs.push_back(ptr);
                    frame.m_paletteGroupID = paletteGroupID;
                    paletteGroupID++;
                }
            }

            // Read sub animation
            {
                uint32_t subAnimPtr = ReadInt(f) + metadataOffset;
                uint32_t rewindOffset = ftell(f);
                if (subAnimPtr <= (uint32_t)ftell(f) - 0x04 || subAnimPtr >= fileSize)
                {
                    _errorMsg = "Invalid sub animation pointer at address " + GetAddressString(ftell(f) - 4);
                    fclose(f);
                    return false;
                }

                fseek(f, subAnimPtr, SEEK_SET);
                uint8_t subAnimCount = ReadInt(f) / 0x04;
                if (subAnimCount > 0xFF)
                {
                    _errorMsg = "Sub animation group has more than 255 animations at address " + GetAddressString(subAnimPtr);
                    fclose(f);
                    return false;
                }

                // Get sub animation pointers
                fseek(f, subAnimPtr, SEEK_SET);
                vector<uint32_t> subAnimPtrs;
                for (uint8_t i = 0; i < subAnimCount; i++)
                {
                    uint32_t ptr = ReadInt(f) + subAnimPtr;
                    if (ptr < (subAnimPtr + subAnimCount * 0x04) || ptr + 6 > fileSize) // Size is at least 3 * 2
                    {
                        _errorMsg = "Invalid sub animation pointer at address " + GetAddressString(ftell(f) - 4);
                        fclose(f);
                        return false;
                    }
                    subAnimPtrs.push_back(ptr);
                }

                // Read sub animations and frames
                frame.m_subAnimations.reserve(subAnimCount);
                for (auto subAnimPtr : subAnimPtrs)
                {
                    fseek(f, subAnimPtr, SEEK_SET);
                    SubAnimation subAnim;

                    uint8_t buffer[3] = { 0x00, 0x00, 0x00 };
                    bool endOfFrame = false;
                    while (!endOfFrame)
                    {
                        if ((uint32_t)ftell(f) >= fileSize)
                        {
                            _errorMsg = "Reached the end of file while reading sub animation data!";
                            fclose(f);
                            return false;
                        }

                        // Object Index
                        buffer[0] = ReadByte(f);

                        // Delay
                        buffer[1] = ReadByte(f);
                        if (buffer[1] == 0x00)
                        {
                            _errorMsg = "Invalid sub animation delay at address " + GetAddressString(ftell(f) - 1);
                            fclose(f);
                            return false;
                        }

                        // Flag
                        buffer[2] = ReadByte(f);
                        subAnim.m_loop = (buffer[2] & 0x40);
                        endOfFrame = (buffer[2] & 0xC0) > 0;
                        if (buffer[2] != 0x00 && (buffer[2] & ~(0xC0)) > 0)
                        {
                            _errorMsg = "Invalid end of sub animation flag at address " + GetAddressString(ftell(f) - 1);
                            fclose(f);
                            return false;
                        }

                        SubFrame frame;
                        frame.m_objectIndex = buffer[0];
                        frame.m_delay = buffer[1];
                        subAnim.m_subFrames.push_back(frame);
                    }
                    frame.m_subAnimations.push_back(subAnim);
                }

                // Jump back to frame data
                fseek(f, rewindOffset, SEEK_SET);

                // Debug info
                /*
                for (uint32_t i = 0; i < frame.m_subAnimations.size(); i++)
                {
                    SubAnimation const& subAnim = frame.m_subAnimations[i];
                    std::cout << "Sub Animation " << i << ": " << (subAnim.m_loop ? "(loop)" : "") << endl;
                    std::cout << left
                        << setw(8) << " "
                        << setw(8) << "Frame"
                        << setw(11) << "ObjIndex"
                        << setw(9) << "Delay" << endl;
                    for (uint32_t j = 0; j < subAnim.m_subFrames.size(); j++)
                    {
                        SubFrame const& frame = subAnim.m_subFrames[j];
                        std::cout << left
                            << setw(8) << " "
                            << setw(8) << to_string(j)
                            << setw(11) << to_string(frame.m_objectIndex)
                            << setw(9) << to_string(frame.m_delay) << endl;
                    }
                }
                */
            }

            // Read object
            {
                uint32_t objectListPtr = ReadInt(f) + metadataOffset;
                uint32_t rewindOffset = ftell(f);
                if (objectListPtr  <= (uint32_t)ftell(f) - 0x04 || objectListPtr >= fileSize)
                {
                    _errorMsg = "Invalid object group pointer at address " + GetAddressString(ftell(f) - 4);
                    fclose(f);
                    return false;
                }

                fseek(f, objectListPtr, SEEK_SET);
                uint8_t objectCount = ReadInt(f) / 0x04;
                if (objectCount > 0xFF)
                {
                    _errorMsg = "Object group has more than 255 objects at address " + GetAddressString(objectListPtr);
                    fclose(f);
                    return false;
                }

                // Get object pointers
                fseek(f, objectListPtr, SEEK_SET);
                vector<uint32_t> objectPtrs;
                for (uint8_t i = 0; i < objectCount; i++)
                {
                    uint32_t ptr = ReadInt(f) + objectListPtr;
                    if (ptr < (objectListPtr + objectCount * 0x04) || ptr + 10 > fileSize) // Size is at least 5 * 2
                    {
                        _errorMsg = "Invalid object pointer at address " + GetAddressString(ftell(f) - 4);
                        fclose(f);
                        return false;
                    }
                    objectPtrs.push_back(ptr);
                }

                // Read sub animations and frames
                frame.m_objects.reserve(objectCount);
                for (auto objectPtr : objectPtrs)
                {
                    fseek(f, objectPtr, SEEK_SET);
                    Object object;

                    uint8_t buffer[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
                    bool endOfFrame = false;
                    while (!endOfFrame)
                    {
                        if ((uint32_t)ftell(f) >= fileSize)
                        {
                            _errorMsg = "Reached the end of file while reading sub animation data!";
                            fclose(f);
                            return false;
                        }

                        buffer[0] = ReadByte(f);
                        buffer[1] = ReadByte(f);
                        buffer[2] = ReadByte(f);
                        buffer[3] = ReadByte(f);
                        buffer[4] = ReadByte(f);

                        if ((buffer[0] == 0xFF && buffer[1] == 0xFF && buffer[2] == 0xFF && buffer[3] == 0xFF && buffer[4] == 0xFF) || (uint32_t)ftell(f) >= fileSize)
                        {
                            endOfFrame = true;
                        }
                        else
                        {
                            SubObject subObject;
                            subObject.m_startTile = buffer[0];
                            subObject.m_posX = static_cast<int8_t>(buffer[1]);
                            subObject.m_posY = static_cast<int8_t>(buffer[2]);
                            subObject.m_hFlip = buffer[3] & 0x40;
                            subObject.m_vFlip = buffer[3] & 0x80;
                            object.m_paletteIndex = buffer[4] >> 4;

                            uint8_t sizeShape = ((buffer[3] & 0x03) << 0x04) | (buffer[4] & 0x03);
                            switch (sizeShape)
                            {
                            case 0x00:
                                subObject.m_sizeX = 8;
                                subObject.m_sizeY = 8;
                                break;
                            case 0x01:
                                subObject.m_sizeX = 16;
                                subObject.m_sizeY = 8;
                                break;
                            case 0x02:
                                subObject.m_sizeX = 8;
                                subObject.m_sizeY = 16;
                                break;
                            case 0x10:
                                subObject.m_sizeX = 16;
                                subObject.m_sizeY = 16;
                                break;
                            case 0x11:
                                subObject.m_sizeX = 32;
                                subObject.m_sizeY = 8;
                                break;
                            case 0x12:
                                subObject.m_sizeX = 8;
                                subObject.m_sizeY = 32;
                                break;
                            case 0x20:
                                subObject.m_sizeX = 32;
                                subObject.m_sizeY = 32;
                                break;
                            case 0x21:
                                subObject.m_sizeX = 32;
                                subObject.m_sizeY = 16;
                                break;
                            case 0x22:
                                subObject.m_sizeX = 16;
                                subObject.m_sizeY = 32;
                                break;
                            case 0x30:
                                subObject.m_sizeX = 64;
                                subObject.m_sizeY = 64;
                                break;
                            case 0x31:
                                subObject.m_sizeX = 64;
                                subObject.m_sizeY = 32;
                                break;
                            case 0x32:
                                subObject.m_sizeX = 32;
                                subObject.m_sizeY = 64;
                                break;
                            default:
                                _errorMsg = "Unexpected OAM dimension at address " + GetAddressString(ftell(f) - 5);
                                fclose(f);
                                return false;
                            }

                            // Check for unused data
                            if ((buffer[3] & 0x3C) > 0)
                            {
                                _errorMsg = "Unused bit 2-5 has been used for flag at address " + GetAddressString(ftell(f) - 2);
                                fclose(f);
                                return false;
                            }
                            if ((buffer[4] & 0x0C) > 0)
                            {
                                _errorMsg = "Unused bit 2-3 has been used for flag at address " + GetAddressString(ftell(f) - 1);
                                fclose(f);
                                return false;
                            }

                            object.m_subObjects.push_back(subObject);
                        }
                    }
                    frame.m_objects.push_back(object);
                }

                // Jump back to frame data
                fseek(f, rewindOffset, SEEK_SET);

                // Debug info
                /*
                for (uint32_t i = 0; i < frame.m_objects.size(); i++)
                {
                    Object const& object = frame.m_objects[i];
                    std::cout << "Object " << i << ":" << endl;
                    std::cout << left
                        << setw(8) << " "
                        << setw(9) << "SubObj"
                        << setw(7) << "Size"
                        << setw(8) << "X-Pos"
                        << setw(8) << "Y-Pos"
                        << setw(9) << "H-Flip"
                        << setw(9) << "V-Flip"
                        << setw(16) << "Palette Index" << endl;
                    for (uint32_t j = 0; j < object.m_subObjects.size(); j++)
                    {
                        SubObject const& subObject = object.m_subObjects[j];
                        std::cout << left
                            << setw(8) << " "
                            << setw(9) << to_string(j)
                            << setw(7) << to_string(subObject.m_sizeX) + "x" + to_string(subObject.m_sizeY)
                            << setw(8) << to_string(subObject.m_posX)
                            << setw(8) << to_string(subObject.m_posY)
                            << setw(9) << (subObject.m_hFlip ? "Yes" : "No")
                            << setw(9) << (subObject.m_vFlip ? "Yes" : "No")
                            << setw(16) << to_string(object.m_paletteIndex) << endl;
                    }
                }
                */
            }

            // Read delay an
            frame.m_delay = ReadByte(f);
            if (frame.m_delay == 0x00)
            {
                _errorMsg = "Invalid frame delay at address " + GetAddressString(ftell(f) - 1);
                fclose(f);
                return false;
            }

            fseek(f, 0x01, SEEK_CUR);

            // Read end of frame flag
            uint8_t flag = ReadByte(f);
            frame.m_specialFlag0 = (flag & 0x01);
            frame.m_specialFlag1 = (flag & 0x02);
            animation.m_loop = (flag & 0x40);
            endOfFrame = (flag & 0xC0) > 0;
            if (flag != 0x00 && (flag & ~(0xC3)) > 0)
            {
                _errorMsg = "Invalid end of frame flag at address " + GetAddressString(ftell(f) - 1);
                fclose(f);
                return false;
            }

            fseek(f, 0x01, SEEK_CUR);

            // Insert frame to animation
            animation.m_frames.push_back(frame);
        }
        m_animations.push_back(animation);

        // Debug info
        /*
        std::cout << endl << "Animation ID " << m_animations.size() - 1 << ": " << animation.m_frames.size() << " frames";
        std::cout << (animation.m_loop ? " (loop)" : "") << endl;

        std::cout << left
            << setw(8) << " "
            << setw(8) << "Frame"
            << setw(9) << "Tileset"
            << setw(9) << "Palette"
            << setw(8) << "Delay" << endl;
        for (uint32_t i = 0; i < animation.m_frames.size(); i++)
        {
            Frame const& frame = animation.m_frames[i];
            std::cout << left
                << setw(8) << " "
                << setw(8) << to_string(i)
                << setw(9) << to_string(frame.m_tilesetID)
                << setw(9) << to_string(frame.m_paletteGroupID)
                << setw(8) << to_string(frame.m_delay) << endl;
        }
        std::cout << "-----------------------------------------------" << endl;
        */
    }

    // Read tileset data
    std::cout << "Reading tileset data..." << endl;
    m_tilesets.reserve(tilesetPtrs.size());
    for (auto tilesetPtr : tilesetPtrs)
    {
        fseek(f, tilesetPtr, SEEK_SET);

        uint32_t size = ReadInt(f);
        Tileset tileset;
        tileset.m_data.reserve(size);

        if ((uint32_t)ftell(f) + size > fileSize)
        {
            _errorMsg = "Error reading tileset at address " + GetAddressString(ftell(f) - 4);
            fclose(f);
            return false;
        }

        for (uint32_t i = 0; i < size; i++)
        {
            tileset.m_data.push_back(ReadByte(f));
        }
        m_tilesets.push_back(tileset);
    }

    // Debug info
    /*
    std::cout << left
        << setw(10) << "Tileset"
        << setw(7) << "Size" << endl;
    for (uint32_t i = 0; i < m_tilesets.size(); i++)
    {
        std::cout << left
            << setw(10) << to_string(i)
            << setw(7) << to_string(m_tilesets[i].m_data.size()) << endl;
    }
    std::cout << endl;
    */

    // Read palette data
    std::cout << "Reading palette data..." << endl;
    m_paletteGroups.reserve(paletteGroupPtrs.size());
    for (uint32_t i = 0; i < paletteGroupPtrs.size(); i++)
    {
        uint32_t const& palettePtr = paletteGroupPtrs[i];

        uint32_t nextPalettePtr = 0xFFFFFFFF;
        for (uint32_t const& ptr : paletteGroupPtrs)
        {
            if (ptr > palettePtr && ptr < nextPalettePtr)
            {
                nextPalettePtr = ptr;
            }
        }

        // Skip 20 00 00 00
        fseek(f, palettePtr + 0x04, SEEK_SET);
        PaletteGroup group;

        // Search for palette until invalid color is found
        bool valid = true;
        while (valid && group.m_palettes.size() < 0xFFFF && (uint32_t)ftell(f) < nextPalettePtr && (uint32_t)ftell(f) < fileSize)
        {
            Palette palette;
            palette.m_colors.reserve(0x10);
            for (uint8_t j = 0; j < 0x10; j++)
            {
                uint16_t color = ReadShort(f);
                if (color & 0x8000)
                {
                    // Palette doesn't allow most significant bit set
                    valid = false;
                    break;
                }
                else
                {
                    palette.m_colors.push_back(color);
                }
            }

            if (valid)
            {
                group.m_palettes.push_back(palette);
            }
        }

        if (group.m_palettes.empty())
        {
            _errorMsg = "Invalid palette at address " + GetAddressString(ftell(f) - 2);
            fclose(f);
            return false;
        }

        m_paletteGroups.push_back(group);
    }

    fclose(f);
    m_loaded = true;
    return true;
}

bool BNSprite::LoadSF
(
    wstring const& _fileName,
    string& _errorMsg
)
{
    typedef struct
    {
        uint16_t tileNum;
        uint16_t tileCount;
    } TilesetInfo;

    Clear();

    FILE* f;
    _wfopen_s(&f, _fileName.c_str(), L"rb");
    if (!f)
    {
        _errorMsg = "Unable to open file!";
        fclose(f);
        return false;
    }

    // File size
    fseek(f, 0, SEEK_END);
    uint32_t fileSize = ftell(f);

    // Read file header
    fseek(f, 0, SEEK_SET);
    uint32_t tsetHdrOffs = ReadInt(f);
    uint32_t palsHdrOffs = ReadInt(f);
    uint32_t animHdrOffs = ReadInt(f);
    uint32_t sprsHdrOffs = ReadInt(f);
    uint32_t tnumShift   = ReadInt(f);

    uint16_t palCount = 0;

    // Read sprites header
    std::cout << "Reading sprites header..." << endl;
    fseek(f, sprsHdrOffs, SEEK_SET);
    uint16_t spriteCount = ReadShort(f);
    fseek(f, 2, SEEK_CUR); // skip padding (?)

    // Read sprite pointers
    std::cout << "Reading sprite pointers..." << endl;
    vector<uint32_t> spritePtrs;
    vector<Frame> sprites;
    vector<bool> spriteUsed;
    spritePtrs.reserve(spriteCount);
    sprites.reserve(spriteCount);
    spriteUsed.reserve(spriteCount);
    for (size_t i = 0; i < spriteCount; i++)
    {
        uint32_t ptr = ReadInt(f) + sprsHdrOffs;
        spritePtrs.push_back(ptr);
    }

    // Read sprites
    std::cout << "Reading sprites..." << endl;
    for (size_t i = 0; i < spriteCount; i++)
    {
        fseek(f, spritePtrs[i], SEEK_SET);
        Frame sprite;

        // Read objects
        Object obj;
        bool last = false;
        do
        {
            SubObject subObj;
            subObj.m_startTile = ReadByte(f) << tnumShift;
            subObj.m_posX = (int8_t)ReadByte(f);
            subObj.m_posY = (int8_t)ReadByte(f);

            uint8_t size = ReadByte(f);
            uint8_t shape = ReadByte(f);
            uint8_t sizeShape = ((size & 0x3) << 0x4) | (shape & 0x3);
            switch (sizeShape)
            {
            case 0x00: subObj.m_sizeX =  8; subObj.m_sizeY =  8; break;
            case 0x01: subObj.m_sizeX = 16; subObj.m_sizeY =  8; break;
            case 0x02: subObj.m_sizeX =  8; subObj.m_sizeY = 16; break;
            case 0x10: subObj.m_sizeX = 16; subObj.m_sizeY = 16; break;
            case 0x11: subObj.m_sizeX = 32; subObj.m_sizeY =  8; break;
            case 0x12: subObj.m_sizeX =  8; subObj.m_sizeY = 32; break;
            case 0x20: subObj.m_sizeX = 32; subObj.m_sizeY = 32; break;
            case 0x21: subObj.m_sizeX = 32; subObj.m_sizeY = 16; break;
            case 0x22: subObj.m_sizeX = 16; subObj.m_sizeY = 32; break;
            case 0x30: subObj.m_sizeX = 64; subObj.m_sizeY = 64; break;
            case 0x31: subObj.m_sizeX = 64; subObj.m_sizeY = 32; break;
            case 0x32: subObj.m_sizeX = 32; subObj.m_sizeY = 64; break;
            }

            uint8_t flip = ReadByte(f);
            subObj.m_hFlip = flip & 0x1;
            subObj.m_vFlip = flip & 0x2;

            last = ReadByte(f);

            subObj.m_startTile += (ReadByte(f) << 8);

            obj.m_subObjects.push_back(subObj);
        }
        while (!last);
        sprite.m_objects.push_back(obj);

        // Create dummy subanimation
        SubFrame subFrame;
        SubAnimation subAnim;
        subFrame.m_objectIndex = 0;
        subFrame.m_delay = 1;
        subAnim.m_loop = true;
        subAnim.m_subFrames.push_back(subFrame);
        sprite.m_subAnimations.push_back(subAnim);

        sprites.push_back(sprite);
        spriteUsed.push_back(false);
    }

    // Read tilesets header
    std::cout << "Reading tilesets header..." << endl;
    fseek(f, tsetHdrOffs, SEEK_SET);
    uint16_t maxTileCount = ReadShort(f);
    uint16_t totalTileCount = ReadShort(f);
    uint16_t tsetHdrSize = ReadShort(f);
    fseek(f, 2, SEEK_CUR); // skip padding (?)

    // Read tilesets entries
    std::cout << "Reading tilesets entries..." << endl;
    vector<TilesetInfo> tsetEntries;
    tsetEntries.reserve(spriteCount);
    for (size_t i = 0; i < spriteCount; i++)
    {
        TilesetInfo entry;
        entry.tileCount = ReadShort(f);
        entry.tileNum   = ReadShort(f);

        // Check for duplicate entries
        auto it = find_if(tsetEntries.begin(), tsetEntries.end(), [&entry] (const auto& x)
        {
            return x.tileNum == entry.tileNum && x.tileCount == entry.tileCount;
        });
        if (it != tsetEntries.end())
        {
            // Use existing ID
            sprites[i].m_tilesetID = it - tsetEntries.begin();
        }
        else
        {
            // Create new ID
            sprites[i].m_tilesetID = tsetEntries.size();
            tsetEntries.push_back(entry);
        }
    }

    // Read tilesets
    std::cout << "Reading tilesets..." << endl;
    for (auto tsetEntry : tsetEntries)
    {
        fseek(f, tsetHdrOffs + tsetHdrSize + tsetEntry.tileNum * 0x20, SEEK_SET);

        Tileset tset;
        uint32_t tsetSize = tsetEntry.tileCount * 0x20;
        tset.m_data.reserve(tsetSize);

        for (uint32_t i = 0; i < tsetSize; i++)
        {
            tset.m_data.push_back(ReadByte(f));
        }
        m_tilesets.push_back(tset);
    }

    // Read animations
    std::cout << "Reading animations header..." << endl;
    fseek(f, animHdrOffs, SEEK_SET);
    uint16_t animCount = ReadShort(f);
    fseek(f, 2, SEEK_CUR); // skip padding (?)

    // Read animation pointers
    std::cout << "Reading animation pointers..." << endl;
    vector<uint32_t> animPtrs;
    animPtrs.reserve(animCount);
    for (size_t i = 0; i < animCount; i++)
    {
        uint32_t ptr = ReadInt(f) + animHdrOffs;
        animPtrs.push_back(ptr);
    }

    // Read animations
    std::cout << "Reading animations..." << endl;
    m_animations.reserve(animCount);
    for (auto animPtr : animPtrs)
    {
        fseek(f, animPtr, SEEK_SET);

        Animation anim;
        uint8_t loop;
        do
        {
            uint8_t sprIdx = ReadByte(f);
            uint8_t delay = ReadByte(f);
            loop = ReadByte(f);
            uint8_t palIdx = ReadByte(f);

            Frame frame = sprites[sprIdx];
            frame.m_delay = delay;
            frame.m_objects[0].m_paletteIndex = palIdx;
            spriteUsed[sprIdx] = true;

            if (palIdx >= palCount)
            {
                palCount = palIdx + 1;
            }

            anim.m_frames.push_back(frame);
        }
        while (!(loop & 0xC0));
        anim.m_loop = loop & 0x40;

        m_animations.push_back(anim);
    }

    // Make extra animation for all unused sprites
    auto it = find(spriteUsed.begin(), spriteUsed.end(), false);
    if (it != spriteUsed.end())
    {
        Animation anim;

        for (size_t i = 0; i < spriteCount; i++)
        {
            if (spriteUsed[i]) continue;

            anim.m_frames.push_back(sprites[i]);
        }

        m_animations.push_back(anim);
    }

    // Read palettes header
    std::cout << "Reading palettes header..." << endl;
    fseek(f, palsHdrOffs, SEEK_SET);
    PaletteGroup palGrp;
    m_paletteGroups.push_back(palGrp);
    uint16_t colDepth = ReadShort(f);
    uint16_t palSize = ReadShort(f);
    if (colDepth != 5)
    {
        _errorMsg = "Unsupported color mode " + to_string(colDepth);
        return false;
    }

    // Figure out the end of the palettes block (probably animations...)
    long blockEnd = fileSize;
    if (tsetHdrOffs > palsHdrOffs && (long)tsetHdrOffs < blockEnd)
    {
        blockEnd = tsetHdrOffs;
    }
    if (animHdrOffs > palsHdrOffs && (long)animHdrOffs < blockEnd)
    {
        blockEnd = animHdrOffs;
    }
    if (sprsHdrOffs > palsHdrOffs && (long)sprsHdrOffs < blockEnd)
    {
        blockEnd = sprsHdrOffs;
    }

    // Read palettes
    std::cout << "Reading palettes..." << endl;
    for (size_t i = 0; i < palCount || ftell(f) < blockEnd; i++)
    {
        Palette pal;
        for (size_t j = 0; j < palSize; j++)
        {
            pal.m_colors.push_back(ReadShort(f));
        }
        m_paletteGroups[0].m_palettes.push_back(pal);
    }

    fclose(f);
    m_loaded = true;
    return true;
}

//-----------------------------------------------------
// Save a sprite file, return false if fail
//-----------------------------------------------------
bool BNSprite::SaveBN
(
    wstring const& _fileName,
    string& _errorMsg
)
{
    FILE* f;
    _wfopen_s(&f, _fileName.c_str(), L"wb");
    if (!f)
    {
        _errorMsg = "Unable to open file!";
        fclose(f);
        return false;
    }

    // SKIPPED: Largest tileset
    fseek(f, 0x01, SEEK_SET);

    // Write metadata
    WriteByte(f, 0x00);
    WriteByte(f, 0x01);
    WriteByte(f, static_cast<uint8_t>(m_animations.size()));

    // SKIPPED: Animation pointers
    fseek(f, 0x04 * m_animations.size(), SEEK_CUR);

    // SKIPPED: All frame data
    // Also check what tileset, palette is used
    set<uint32_t> tilesetUsed;
    set<uint32_t> paletteUsed;
    uint32_t totalFrameCount = 0;
    for (Animation const& anim : m_animations)
    {
        totalFrameCount += anim.m_frames.size();
        for (Frame const& frame : anim.m_frames)
        {
            tilesetUsed.insert(frame.m_tilesetID);
            paletteUsed.insert(frame.m_paletteGroupID);
        }
    }
    fseek(f, 0x14 * totalFrameCount, SEEK_CUR);

    // Write tileset data
    uint8_t largestTileCount = 0;
    vector<uint32_t> tilesetPtrs;
    tilesetPtrs.reserve(m_tilesets.size());
    for (uint32_t i = 0; i < m_tilesets.size(); i++)
    {
        // Not used, push dummy pointer
        if (tilesetUsed.count(i) == 0)
        {
            tilesetPtrs.push_back(0xFFFFFFFF);
            continue;
        }

        Tileset const& tileset = m_tilesets[i];
        tilesetPtrs.push_back(ftell(f) - 0x04);
        WriteInt(f, tileset.m_data.size());
        for (uint8_t const& byte : tileset.m_data)
        {
            WriteByte(f, byte);
        }

        // Find the largest tile set size
        uint32_t tileCount = tileset.m_data.size() / 0x20;
        tileCount = tileCount > 0xFF ? 0xFF : tileCount;
        largestTileCount = tileCount > largestTileCount ? static_cast<uint8_t>(tileCount) : largestTileCount;
    }
    assert(ftell(f) % 0x04 == 0x00);

    // Write palette data
    vector<uint32_t> palettePtrs;
    palettePtrs.reserve(m_paletteGroups.size());
    for (uint32_t i = 0; i < m_paletteGroups.size(); i++)
    {
        // Not used, push dummy pointer
        if (paletteUsed.count(i) == 0)
        {
            palettePtrs.push_back(0xFFFFFFFF);
            continue;
        }

        PaletteGroup const& paletteGroup = m_paletteGroups[i];
        palettePtrs.push_back(ftell(f) - 0x04);
        WriteInt(f, 0x00000020);
        for (Palette const& palette : paletteGroup.m_palettes)
        {
            for (uint16_t const& color : palette.m_colors)
            {
                WriteShort(f, color);
            }
        }
    }
    assert(ftell(f) % 0x04 == 0x00);

    // Write sub animations
    vector<uint32_t> subAnimGroupPtrs;
    subAnimGroupPtrs.reserve(totalFrameCount);
    for (Animation const& anim : m_animations)
    {
        for (Frame const& frame : anim.m_frames)
        {
            uint32_t subAnimGroupPtr = ftell(f);
            subAnimGroupPtrs.push_back(subAnimGroupPtr - 0x04);

            // SKIPPED: sub animation pointers
            fseek(f, 0x04 * frame.m_subAnimations.size(), SEEK_CUR);

            // Write sub frames
            vector<uint32_t> subAnimPtrs;
            subAnimPtrs.reserve(frame.m_subAnimations.size());
            for (SubAnimation const& subAnim : frame.m_subAnimations)
            {
                subAnimPtrs.push_back(ftell(f) - subAnimGroupPtr);
                for (uint32_t i = 0; i < subAnim.m_subFrames.size(); i++)
                {
                    SubFrame const& subFrame = subAnim.m_subFrames[i];
                    WriteByte(f, subFrame.m_objectIndex);
                    WriteByte(f, subFrame.m_delay);
                    WriteByte(f, i < subAnim.m_subFrames.size() - 1 ? 0x00 : (subAnim.m_loop ? 0xC0 : 0x80));
                }

                // Ends with FF FF FF
                WriteByte(f, 0xFF);
                WriteByte(f, 0xFF);
                WriteByte(f, 0xFF);
            }

            // Go back and write sub animation pointers
            uint32_t endAddress = ftell(f);
            fseek(f, subAnimGroupPtr, SEEK_SET);
            for (uint32_t const& subAnimPtr : subAnimPtrs)
            {
                WriteInt(f, subAnimPtr);
            }
            fseek(f, endAddress, SEEK_SET);
            AlignFourBytes(f);
        }
    }

    // Write objects
    vector<uint32_t> objectGroupPtrs;
    objectGroupPtrs.reserve(totalFrameCount);
    for (uint32_t i = 0; i < m_animations.size(); i++)
    {
        Animation const& anim = m_animations[i];
        for (Frame const& frame : anim.m_frames)
        {
            uint32_t objectGroupPtr = ftell(f);
            objectGroupPtrs.push_back(objectGroupPtr - 0x04);

            // SKIPPED: object pointers
            fseek(f, 0x04 * frame.m_objects.size(), SEEK_CUR);

            // Write sub objects
            vector<uint32_t> objectPtrs;
            objectPtrs.reserve(frame.m_objects.size());
            for (Object const& object : frame.m_objects)
            {
                objectPtrs.push_back(ftell(f) - objectGroupPtr);
                for (uint32_t j = 0; j < object.m_subObjects.size(); j++)
                {
                    SubObject const& subObject = object.m_subObjects[j];
                    WriteByte(f, subObject.m_startTile);
                    WriteByte(f, static_cast<uint8_t>(subObject.m_posX));
                    WriteByte(f, static_cast<uint8_t>(subObject.m_posY));

                    uint8_t size = 0x00;
                    uint8_t shape = 0x00;
                    if (subObject.m_sizeX == 8 && subObject.m_sizeY == 8)
                    {
                        size = 0x00;
                        shape = 0x00;
                    }
                    else if (subObject.m_sizeX == 16 && subObject.m_sizeY == 8)
                    {
                        size = 0x00;
                        shape = 0x01;
                    }
                    else if (subObject.m_sizeX == 8 && subObject.m_sizeY == 16)
                    {
                        size = 0x00;
                        shape = 0x02;
                    }
                    else if (subObject.m_sizeX == 16 && subObject.m_sizeY == 16)
                    {
                        size = 0x01;
                        shape = 0x00;
                    }
                    else if (subObject.m_sizeX == 32 && subObject.m_sizeY == 8)
                    {
                        size = 0x01;
                        shape = 0x01;
                    }
                    else if (subObject.m_sizeX == 8 && subObject.m_sizeY == 32)
                    {
                        size = 0x01;
                        shape = 0x02;
                    }
                    else if (subObject.m_sizeX == 32 && subObject.m_sizeY == 32)
                    {
                        size = 0x02;
                        shape = 0x00;
                    }
                    else if (subObject.m_sizeX == 32 && subObject.m_sizeY == 16)
                    {
                        size = 0x02;
                        shape = 0x01;
                    }
                    else if (subObject.m_sizeX == 16 && subObject.m_sizeY == 32)
                    {
                        size = 0x02;
                        shape = 0x02;
                    }
                    else if (subObject.m_sizeX == 64 && subObject.m_sizeY == 64)
                    {
                        size = 0x03;
                        shape = 0x00;
                    }
                    else if (subObject.m_sizeX == 64 && subObject.m_sizeY == 32)
                    {
                        size = 0x03;
                        shape = 0x01;
                    }
                    else if (subObject.m_sizeX == 32 && subObject.m_sizeY == 64)
                    {
                        size = 0x03;
                        shape = 0x02;
                    }
                    else
                    {
                        assert(false);
                    }

                    uint8_t flag1 = size;
                    if (subObject.m_vFlip)
                    {
                        flag1 |= 0x80;
                    }
                    if (subObject.m_hFlip)
                    {
                        flag1 |= 0x40;
                    }
                    WriteByte(f, flag1);

                    uint8_t flag2 = shape;
                    assert(object.m_paletteIndex <= 0x0F);
                    flag2 |= ((object.m_paletteIndex & 0x0F) << 4);
                    WriteByte(f, flag2);
                }

                // Ends with FF FF FF FF FF
                WriteByte(f, 0xFF);
                WriteByte(f, 0xFF);
                WriteByte(f, 0xFF);
                WriteByte(f, 0xFF);
                WriteByte(f, 0xFF);
            }

            // Go back and write object pointers
            uint32_t endAddress = ftell(f);
            fseek(f, objectGroupPtr, SEEK_SET);
            for (uint32_t const& objectPtr : objectPtrs)
            {
                WriteInt(f, objectPtr);
            }
            fseek(f, endAddress, SEEK_SET);
            AlignFourBytes(f, i < m_animations.size() - 1);
        }
    }

    // Go back and write frame data
    vector<uint32_t> animationPtrs;
    animationPtrs.reserve(m_animations.size());
    fseek(f, 0x04 + 0x04 * m_animations.size(), SEEK_SET);
    uint32_t currentFrameID = 0;
    for (Animation const& animation : m_animations)
    {
        animationPtrs.push_back(ftell(f) - 0x04);
        for (uint32_t i = 0; i < animation.m_frames.size(); i++)
        {
            Frame const& frame = animation.m_frames[i];
            WriteInt(f, tilesetPtrs[frame.m_tilesetID]);
            WriteInt(f, palettePtrs[frame.m_paletteGroupID]);
            WriteInt(f, subAnimGroupPtrs[currentFrameID]);
            WriteInt(f, objectGroupPtrs[currentFrameID]);

            WriteShort(f, frame.m_delay);
            uint16_t flag = 0;
            if (frame.m_specialFlag0)
            {
                flag |= 0x01;
            }
            if (frame.m_specialFlag1)
            {
                flag |= 0x02;
            }
            if (i == animation.m_frames.size() - 1)
            {
                flag |= (animation.m_loop ? 0xC0 : 0x80);
            }
            WriteShort(f, flag);
            currentFrameID++;
        }
    }

    // Go back and write animation pointers
    fseek(f, 0x04, SEEK_SET);
    for (uint8_t i = 0; i < m_animations.size(); i++)
    {
        WriteInt(f, animationPtrs[i]);
    }

    // Go back and write the largest tileset count
    fseek(f, 0x00, SEEK_SET);
    WriteByte(f, largestTileCount);

    fclose(f);
    return true;
}

bool BNSprite::SaveSF
(
    wstring const& _fileName,
    string& _errorMsg
)
{
    typedef struct
    {
        uint16_t tileNum;
        uint16_t tileCount;
    } TilesetInfo;

    FILE* f;
    _wfopen_s(&f, _fileName.c_str(), L"wb");
    if (!f)
    {
        _errorMsg = "Unable to open file!";
        fclose(f);
        return false;
    }

    // Get all unique sprites
    // Also check what tilesets are used
    vector<Frame> sprites;
    vector<vector<size_t>> spriteIdxes;
    vector<Tileset> tsets;
    vector<TilesetInfo> tsetEntries;
    vector<int> tsetIdxes(m_tilesets.size(), -1);
    spriteIdxes.reserve(m_animations.size());
    tsets.reserve(m_tilesets.size());
    tsetEntries.reserve(m_tilesets.size());
    size_t tsetSizeMax = 0;
    size_t tsetSizeTotal = 0;
    for (Animation anim : m_animations)
    {
        vector<size_t> animSpriteIdxes;
        animSpriteIdxes.reserve(anim.m_frames.size());

        for (Frame frame : anim.m_frames)
        {
            // New tileset to be added
            if (tsetIdxes[frame.m_tilesetID] == -1)
            {
                Tileset tset = m_tilesets[frame.m_tilesetID];
                size_t tsetSize = tset.m_data.size() / 0x20;

                // Add tileset
                tsetIdxes[frame.m_tilesetID] = (int)tsets.size();
                tsets.push_back(tset);

                // Create tileset entry
                TilesetInfo entry;
                entry.tileNum = (uint16_t)tsetSizeTotal;
                entry.tileCount = (uint16_t)tsetSize;
                tsetEntries.push_back(entry);

                // Increment counters
                if (tsetSize > tsetSizeMax)
                {
                    tsetSizeMax = tsetSize;
                }
                tsetSizeTotal += tsetSize;
            }

            // Find duplicate sprites
            auto it = find_if(sprites.begin(), sprites.end(), [&frame] (const auto& x)
            {
                // Tileset needs to match
                if (frame.m_tilesetID != x.m_tilesetID)
                {
                    return false;
                }

                // Objects need to match
                if (frame.m_objects.size() != x.m_objects.size())
                {
                    return false;
                }

                // Check each object
                for (size_t i = 0; i < frame.m_objects.size(); i++)
                {
                    if (frame.m_objects[i].m_subObjects.size() != x.m_objects[i].m_subObjects.size())
                    {
                        return false;
                    }

                    for (size_t j = 0; j < frame.m_objects[i].m_subObjects.size(); j++)
                    {
                        SubObject a = frame.m_objects[i].m_subObjects[j];
                        SubObject b = x.m_objects[i].m_subObjects[j];

                        if (a.m_startTile != b.m_startTile ||
                            a.m_posX != b.m_posX ||
                            a.m_posY != b.m_posY ||
                            a.m_sizeX != b.m_sizeX ||
                            a.m_sizeY != b.m_sizeY ||
                            a.m_hFlip != b.m_hFlip ||
                            a.m_vFlip != b.m_vFlip)
                        {
                            return false;
                        }
                    }
                }

                // Note that palette does NOT need to match

                // Sprites are a match
                return true;
            });
            if (it != sprites.end())
            {
                // Use existing ID
                animSpriteIdxes.push_back(it - sprites.begin());
            }
            else
            {
                // Create new ID
                animSpriteIdxes.push_back(sprites.size());
                sprites.push_back(frame);
            }
        }

        spriteIdxes.push_back(animSpriteIdxes);
    }
    if (sprites.size() > 0xFF)
    {
        _errorMsg = "Cannot have more than 255 unique sprites. This file has " + to_string(sprites.size()) + ".";
        return false;
    }

    // Write tilesets header
    fseek(f, 0x14, SEEK_SET);
    uint32_t tsetHdrOffs = ftell(f);
    WriteShort(f, tsetSizeMax);
    WriteShort(f, tsetSizeTotal);
    WriteShort(f, 0x8 + sprites.size() * 0x4); // header size
    AlignFourBytes(f);
    for (Frame sprite : sprites)
    {
        TilesetInfo entry = tsetEntries[tsetIdxes[sprite.m_tilesetID]];
        WriteShort(f, entry.tileCount);
        WriteShort(f, entry.tileNum);
    }

    // Write tilesets
    for (Tileset tset : tsets)
    {
        for (uint8_t b : tset.m_data)
        {
            WriteByte(f, b);
        }
    }
    AlignFourBytes(f);

    // Write palettes header
    uint32_t palsHdrOffs = ftell(f);
    WriteShort(f, 0x5); // color depth
    WriteShort(f, 0x10); // palette size

    // Write palettes
    for (Palette pal : m_paletteGroups[0].m_palettes)
    {
        for (uint16_t c : pal.m_colors)
        {
            WriteShort(f, c);
        }
    }
    AlignFourBytes(f);

    // Write animations header
    uint32_t animHdrOffs = ftell(f);
    WriteShort(f, m_animations.size());
    AlignFourBytes(f);
    uint32_t animPtr = 0x4 + m_animations.size() * 0x4;
    for (Animation anim : m_animations)
    {
        WriteInt(f, animPtr);
        animPtr += anim.m_frames.size() * 0x4;
    }

    // Write animations
    for (size_t i = 0; i < m_animations.size(); i++)
    {
        Animation anim = m_animations[i];
        for (size_t j = 0; j < anim.m_frames.size(); j++)
        {
            Frame frame = anim.m_frames[j];
            size_t sprIdx = spriteIdxes[i][j];

            WriteByte(f, sprIdx);
            WriteByte(f, frame.m_delay);
            if (j == anim.m_frames.size() - 1)
            {
                WriteByte(f, anim.m_loop ? 0x40 : 0x80);
            }
            else
            {
                WriteByte(f, 0x00);
            }
            WriteByte(f, frame.m_objects[0].m_paletteIndex);
        }
    }
    AlignFourBytes(f);

    // Write sprites header
    uint32_t sprsHdrOffs = ftell(f);
    WriteShort(f, sprites.size());
    AlignFourBytes(f);
    uint32_t spritePtr = 0x4 + sprites.size() * 0x4;
    for (Frame sprite : sprites)
    {
        WriteInt(f, spritePtr);
        spritePtr += sprite.m_objects[0].m_subObjects.size() * 0x8;
    }

    // Write sprites
    for (Frame sprite : sprites)
    {
        vector<SubObject> &subObjs = sprite.m_objects[0].m_subObjects;
        for (size_t i = 0; i < subObjs.size(); i++)
        {
            SubObject subObj = subObjs[i];

            uint8_t size = 0;
            uint8_t shape = 0;
            if (subObj.m_sizeX ==  8 && subObj.m_sizeY ==  8) { size = 0; shape = 0; }
            if (subObj.m_sizeX == 16 && subObj.m_sizeY ==  8) { size = 0; shape = 1; }
            if (subObj.m_sizeX ==  8 && subObj.m_sizeY == 16) { size = 0; shape = 2; }
            if (subObj.m_sizeX == 16 && subObj.m_sizeY == 16) { size = 1; shape = 0; }
            if (subObj.m_sizeX == 32 && subObj.m_sizeY ==  8) { size = 1; shape = 1; }
            if (subObj.m_sizeX ==  8 && subObj.m_sizeY == 32) { size = 1; shape = 2; }
            if (subObj.m_sizeX == 32 && subObj.m_sizeY == 32) { size = 2; shape = 0; }
            if (subObj.m_sizeX == 32 && subObj.m_sizeY == 16) { size = 2; shape = 1; }
            if (subObj.m_sizeX == 16 && subObj.m_sizeY == 32) { size = 2; shape = 2; }
            if (subObj.m_sizeX == 64 && subObj.m_sizeY == 64) { size = 3; shape = 0; }
            if (subObj.m_sizeX == 64 && subObj.m_sizeY == 32) { size = 3; shape = 1; }
            if (subObj.m_sizeX == 32 && subObj.m_sizeY == 64) { size = 3; shape = 2; }

            uint8_t flip = 0;
            flip |= subObj.m_hFlip ? 0x1 : 0x0;
            flip |= subObj.m_vFlip ? 0x2 : 0x0;

            bool last = i == subObjs.size() - 1;

            WriteByte(f, subObj.m_startTile >> 0x1);
            WriteByte(f, subObj.m_posX);
            WriteByte(f, subObj.m_posY);
            WriteByte(f, size);
            WriteByte(f, shape);
            WriteByte(f, flip);
            WriteByte(f, last);
            WriteByte(f, subObj.m_startTile >> 0x9);
        }
    }
    AlignFourBytes(f);

    // Write file header
    fseek(f, 0, SEEK_SET);
    WriteInt(f, tsetHdrOffs);
    WriteInt(f, palsHdrOffs);
    WriteInt(f, animHdrOffs);
    WriteInt(f, sprsHdrOffs);
    WriteInt(f, 1); // tile number shift

    fclose(f);
    return true;
}

//-----------------------------------------------------
// Merge with another sprite
//-----------------------------------------------------
bool BNSprite::Merge
(
    const BNSprite &_other,
    string& _errorMsg
)
{
    if (_other.m_animations.size() + m_animations.size() > 255)
    {
        _errorMsg = "Total no. of animations exceed 255!";
        return false;
    }

    // Merge tileset
    int const tilesetIDStart = m_tilesets.size();
    for (Tileset const& tileset : _other.m_tilesets)
    {
        m_tilesets.push_back(tileset);
    }

    // Merge palette groups
    int const paletteIDStart = m_paletteGroups.size();
    for (PaletteGroup const& group : _other.m_paletteGroups)
    {
        m_paletteGroups.push_back(group);
    }

    // Merge animations, fix tileset and palette ID
    for (Animation anim : _other.m_animations)
    {
        for (Frame& frame : anim.m_frames)
        {
            frame.m_tilesetID += tilesetIDStart;
            frame.m_paletteGroupID += paletteIDStart;
        }
        m_animations.push_back(anim);
    }

    return true;
}

//-----------------------------------------------------
// Get all frames in an animation
//-----------------------------------------------------
void BNSprite::GetAnimationFrames
(
    int _animID,
    vector<BNSprite::Frame> &_frames
)
{
    _frames.clear();
    if (_animID < 0 || _animID >= m_animations.size()) return;

    Animation const& anim = m_animations[_animID];
    for(Frame const& frame : anim.m_frames)
    {
        _frames.push_back(frame);
    }
}

//-----------------------------------------------------
// Get tileset data from ID
//-----------------------------------------------------
void BNSprite::GetTilesetPixels
(
    int _tilesetID,
    vector<uint8_t> &_data
)
{
    _data.clear();
    if (_tilesetID < 0 || _tilesetID >= m_tilesets.size()) return;

    for (uint8_t const& byte : m_tilesets[_tilesetID].m_data)
    {
        _data.push_back(byte & 0xF);
        _data.push_back((byte & 0xF0) >> 4);
    }
}

//-----------------------------------------------------
// Get all available palettes
//-----------------------------------------------------
void BNSprite::GetAllPaletteGroups
(
    vector<BNSprite::PaletteGroup> &_paletteGroups
)
{
    _paletteGroups.clear();
    for (PaletteGroup const& group : m_paletteGroups)
    {
        _paletteGroups.push_back(group);
    }
}

//-----------------------------------------------------
// Replace all palettes
//-----------------------------------------------------
void BNSprite::ReplaceAllPaletteGroups
(
    vector<BNSprite::PaletteGroup> const&_paletteGroups
)
{
    m_paletteGroups.clear();
    for (PaletteGroup const& group : _paletteGroups)
    {
        m_paletteGroups.push_back(group);
    }
}

//-----------------------------------------------------
// Get first frames for all animations
//-----------------------------------------------------
BNSprite::Frame BNSprite::GetAnimationFrame
(
    int _animID,
    int _frameID
)
{
    if (_animID < 0 || _animID >= m_animations.size())
    {
        assert(false);
        return Frame();
    }

    Animation const& anim = m_animations[_animID];
    if (_frameID < 0 || _frameID >= anim.m_frames.size())
    {
        assert(false);
        return Frame();
    }

    return anim.m_frames[_frameID];
}

//-----------------------------------------------------
// Create new animation
//-----------------------------------------------------
int BNSprite::NewAnimation
(
    int _copyFrom // = -1
)
{
    if (_copyFrom == -1 && m_animations.size() < 256)
    {
        SubAnimation subAnim;
        subAnim.m_subFrames.push_back(SubFrame());
        Object object;
        object.m_subObjects.push_back(SubObject());

        Frame frame;
        frame.m_subAnimations.push_back(subAnim);
        frame.m_objects.push_back(object);

        Animation anim;
        anim.m_frames.push_back(frame);
        m_animations.push_back(anim);

        return m_animations.size() - 1;
    }
    else if (_copyFrom >= 0 && _copyFrom < m_animations.size())
    {
        Animation const& anim = m_animations[_copyFrom];
        m_animations.push_back(anim);

        return m_animations.size() - 1;
    }

    return -1;
}

//-----------------------------------------------------
// Swap two animations
//-----------------------------------------------------
void BNSprite::SwapAnimations
(
    int _id1,
    int _id2
)
{
    if (_id1 < 0 || _id1 >= m_animations.size()) return;
    if (_id2 < 0 || _id2 >= m_animations.size()) return;
    iter_swap(m_animations.begin() + _id1, m_animations.begin() + _id2);
}

//-----------------------------------------------------
// Delete animation
//-----------------------------------------------------
void BNSprite::DeleteAnimation
(
    int _animID
)
{
    if (_animID < 0 || _animID >= m_animations.size()) return;
    m_animations.erase(m_animations.begin() + _animID);
}

//-----------------------------------------------------
// Set if animation loops
//-----------------------------------------------------
void BNSprite::SetAnimationLoop
(
    int _animID,
    bool _loop
)
{
    if (_animID < 0 || _animID >= m_animations.size()) return;
    m_animations[_animID].m_loop = _loop;
}

//-----------------------------------------------------
// Create new frame
//-----------------------------------------------------
int BNSprite::NewFrame
(
    int _animID,
    int _copyAnimID, //= -1
    int _copyFrameID //= -1
)
{
    if (_animID < 0 || _animID >= m_animations.size()) return -1;
    Animation& anim = m_animations[_animID];

    if (_copyAnimID == -1)
    {
        SubAnimation subAnim;
        subAnim.m_subFrames.push_back(SubFrame());
        Object object;
        object.m_subObjects.push_back(SubObject());

        Frame frame;
        frame.m_subAnimations.push_back(subAnim);
        frame.m_objects.push_back(object);
        anim.m_frames.push_back(frame);

        return anim.m_frames.size() - 1;
    }
    else
    {
        if (_copyFrameID < 0 || _copyAnimID >= m_animations.size()) return -1;
        Animation const& copyAnim = m_animations[_copyAnimID];

        if (_copyFrameID >= copyAnim.m_frames.size()) return -1;

        Frame const& frame = copyAnim.m_frames[_copyFrameID];
        anim.m_frames.push_back(frame);

        return anim.m_frames.size() - 1;
    }
}

//-----------------------------------------------------
// Swap two frames
//-----------------------------------------------------
void BNSprite::SwapFrames
(
    int _animID,
    int _id1,
    int _id2
)
{
    if (_animID < 0 || _animID >= m_animations.size()) return;
    Animation& anim = m_animations[_animID];

    if (_id1 < 0 || _id1 >= anim.m_frames.size()) return;
    if (_id2 < 0 || _id2 >= anim.m_frames.size()) return;
    iter_swap(anim.m_frames.begin() + _id1, anim.m_frames.begin() + _id2);
}

//-----------------------------------------------------
// Delete frame
//-----------------------------------------------------
void BNSprite::DeleteFrame
(
    int _animID,
    int _frameID
)
{
    if (_animID < 0 || _animID >= m_animations.size()) return;
    Animation& anim = m_animations[_animID];

    if (_frameID < 0 || _frameID >= anim.m_frames.size()) return;
    anim.m_frames.erase(anim.m_frames.begin() + _frameID);
}

//-----------------------------------------------------
// Replace frame
//-----------------------------------------------------
void BNSprite::ReplaceFrame
(
    int _animID,
    int _frameID,
    BNSprite::Frame const& _frame
)
{
    if (_animID < 0 || _animID >= m_animations.size()) return;
    Animation& anim = m_animations[_animID];

    if (_frameID < 0 || _frameID >= anim.m_frames.size()) return;

    anim.m_frames.push_back(_frame);
    iter_swap(anim.m_frames.begin() + _frameID, anim.m_frames.end() - 1);
    anim.m_frames.pop_back();

    std::cout << "Anim " << to_string(_animID) << " Frame " << to_string(_frameID) << " saved!" << endl;
}

//-----------------------------------------------------
// Replace a tileset
//-----------------------------------------------------
bool BNSprite::ImportTileset
(
    int _tilesetID,
    const wstring &_fileName,
    string &_errorMsg
)
{
    if (_tilesetID < 0 || _tilesetID >= m_tilesets.size())
    {
        _errorMsg = "Invalid tileset ID!";
        return false;
    }

    FILE* f;
    _wfopen_s(&f, _fileName.c_str(), L"rb");
    if (!f)
    {
        _errorMsg = "Unable to open file!";
        fclose(f);
        return false;
    }

    // File size
    fseek(f, 0, SEEK_END);
    uint32_t fileSize = ftell(f);

    if (fileSize == 0)
    {
        _errorMsg = "File is empty!";
        fclose(f);
        return false;
    }

    if (fileSize % 0x20 != 0)
    {
        _errorMsg = "Invalid tileset file, size must be multiple of 0x20 bytes!";
        fclose(f);
        return false;
    }

    // Overwrite tileset
    Tileset& tileset = m_tilesets[_tilesetID];
    tileset.m_data.clear();
    fseek(f, 0, SEEK_SET);
    for (uint32_t i = 0; i < fileSize; i++)
    {
        tileset.m_data.push_back(ReadByte(f));
    }

    fclose(f);
    return true;
}

//-----------------------------------------------------
// Export a tileset
//-----------------------------------------------------
bool BNSprite::ExportTileset
(
    int _tilesetID,
    const wstring &_fileName,
    string &_errorMsg
)
{
    if (_tilesetID < 0 || _tilesetID >= m_tilesets.size())
    {
        _errorMsg = "Invalid tileset ID!";
        return false;
    }

    FILE* f;
    _wfopen_s(&f, _fileName.c_str(), L"wb");
    if (!f)
    {
        _errorMsg = "Unable to open file!";
        fclose(f);
        return false;
    }

    for (uint8_t const& byte : m_tilesets[_tilesetID].m_data)
    {
        WriteByte(f, byte);
    }

    fclose(f);
    return true;
}

//-----------------------------------------------------
// Create a new tileset for custom sprite
//-----------------------------------------------------
void BNSprite::ImportCustomTileset(const vector<uint8_t> &_data)
{
    m_loaded = true;
    m_tilesets.push_back(Tileset());

    Tileset& tileset = m_tilesets[m_tilesets.size() - 1];
    tileset.m_data.reserve(_data.size());
    tileset.m_data.assign(_data.begin(), _data.end());
}

//-----------------------------------------------------
// Convert GBA color to RGB
//-----------------------------------------------------
uint32_t BNSprite::GBAtoRGB
(
    uint16_t _color
)
{
    // GBA format is BGR555!!!
    uint32_t a = 0xFF;
    uint32_t r = _color & 0x1F;
    uint32_t g = (_color >> 5) & 0x1F;
    uint32_t b = (_color >> 10) & 0x1F;

    uint32_t rScale = (uint32_t)(((double)(r * 0xFF + 0xF)) / 0x1F);
    uint32_t gScale = (uint32_t)(((double)(g * 0xFF + 0xF)) / 0x1F);
    uint32_t bScale = (uint32_t)(((double)(b * 0xFF + 0xF)) / 0x1F);

    return (a << 24) | (rScale << 16) | (gScale << 8) | bScale;
}

//-----------------------------------------------------
// Convert RGB to GBA color
//-----------------------------------------------------
uint16_t BNSprite::RGBtoGBA
(
    uint32_t _color
)
{
    uint32_t r = (_color & 0xFF0000) >> 16;
    uint32_t g = (_color & 0xFF00) >> 8;
    uint32_t b = (_color & 0xFF);

    uint32_t rScale = (uint16_t)(((double)(r * 0x1F + 0x7F)) / 0xFF);
    uint32_t gScale = (uint16_t)(((double)(g * 0x1F + 0x7F)) / 0xFF);
    uint32_t bScale = (uint16_t)(((double)(b * 0x1F + 0x7F)) / 0xFF);

    // GBA format is BGR555!!!
    return (bScale << 10) | (gScale << 5) | rScale;
}

//-----------------------------------------------------
// Convert RGB to GBA and back
//-----------------------------------------------------
uint32_t BNSprite::ClampRGB(uint32_t _color)
{
    return GBAtoRGB(RGBtoGBA(_color));
}

//-----------------------------------------------------
// Read a byte
//-----------------------------------------------------
uint8_t BNSprite::ReadByte
(
    FILE* _file
)
{
    uint8_t byte;
    fread(&byte, 1, 1, _file);
    return byte;
}

//-----------------------------------------------------
// Read a short from 2 bytes
//-----------------------------------------------------
uint16_t BNSprite::ReadShort
(
    FILE* _file
)
{
    // Read short, require flipping bytes
    uint16_t value;
    fread(&value, 2, 1, _file);
    return value;
}

//-----------------------------------------------------
// Read an int from 4 bytes
//-----------------------------------------------------
uint32_t BNSprite::ReadInt
(
    FILE* _file
)
{
    // Read int, require flipping bytes
    uint32_t value;
    fread(&value, 4, 1, _file);
    return value;
}

//-----------------------------------------------------
// Write a byte
//-----------------------------------------------------
void BNSprite::WriteByte
(
    FILE* _file,
    uint8_t _writeByte
)
{
    fwrite(&_writeByte, 1, 1, _file);
}

//-----------------------------------------------------
// Write 2 bytes from short
//-----------------------------------------------------
void BNSprite::WriteShort
(
    FILE* _file,
    uint16_t _writeShort
)
{
    fwrite(&_writeShort, 2, 1, _file);
}

//-----------------------------------------------------
// Write 4 bytes from int
//-----------------------------------------------------
void BNSprite::WriteInt
(
    FILE* _file,
    uint32_t _writeInt
)
{
    fwrite(&_writeInt, 4, 1, _file);
}

//-----------------------------------------------------
// Align offset to 4 bytes
//-----------------------------------------------------
void BNSprite::AlignFourBytes
(
    FILE* _file,
    bool _padExtra
)
{
    // At least pad one byte, why
    if (_padExtra)
    {
        WriteByte(_file, 0x00);
    }

    while (ftell(_file) % 0x04)
    {
        WriteByte(_file, 0x00);
    }
}

//-----------------------------------------------------
// Return string of current addres
//-----------------------------------------------------
string BNSprite::GetAddressString
(
    uint32_t _address
)
{
    stringstream stream;
    stream << "0x" << setfill('0') << setw(8);
    stream << hex << _address;
    return stream.str();
}
