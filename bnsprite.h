#ifndef BNSPRITE_H
#define BNSPRITE_H

#include <string>
#include <vector>
#include <set>
#include <map>

using namespace std;

class BNSprite
{
public:
    struct SubObject
    {
        uint16_t m_startTile;
        int8_t m_posX;
        int8_t m_posY;
        bool m_hFlip;
        bool m_vFlip;
        int32_t m_sizeX;
        int32_t m_sizeY;

        SubObject()
            : m_startTile(0)
            , m_posX(-4)
            , m_posY(-4)
            , m_hFlip(false)
            , m_vFlip(false)
            , m_sizeX(8)
            , m_sizeY(8)
        {}
    };

    struct Object
    {
        uint8_t m_paletteIndex;
        vector<SubObject> m_subObjects;

        Object()
            : m_paletteIndex(0)
        {}
    };

    struct SubFrame
    {
        uint8_t m_objectIndex;
        uint8_t m_delay;

        SubFrame()
            : m_objectIndex(0)
            , m_delay(1)
        {}
    };

    struct SubAnimation
    {
        bool m_loop;
        vector<SubFrame> m_subFrames;

        SubAnimation()
            : m_loop(false)
        {}
    };

    struct Palette
    {
        vector<uint16_t> m_colors;
    };

    struct PaletteGroup
    {
        vector<Palette> m_palettes;
    };

    struct Tileset
    {
        vector<uint8_t> m_data;
    };

    struct Frame
    {
        bool m_specialFlag0;	// 0x01 - sfx for jack-in
        bool m_specialFlag1;	// 0x02 - sfx for jack-in

        uint32_t m_tilesetID;
        uint32_t m_paletteGroupID;
        vector<SubAnimation> m_subAnimations;
        vector<Object> m_objects;

        uint8_t m_delay;

        Frame()
            : m_specialFlag0(false)
            , m_specialFlag1(false)
            , m_tilesetID(0)
            , m_paletteGroupID(0)
            , m_delay(1)
        {}
    };

    struct Animation
    {
        bool m_loop;
        vector<Frame> m_frames;

        Animation()
            : m_loop(false)
        {}
    };

public:
    BNSprite();
    ~BNSprite();

    bool IsLoaded() { return m_loaded; }
    void Clear();

    // Load & Save
    bool LoadBN(wstring const& _fileName, string& _errorMsg);
    bool LoadSF(wstring const& _fileName, string& _errorMsg);
    bool Save(wstring const& _fileName, string& _errorMsg);

    // Merge with another sprite
    bool Merge(BNSprite const& _other, string& _errorMsg);

    // Helper
    int GetAnimationCount() { return m_animations.size(); }
    int GetAnimationFrameCount(int _animID) { return m_animations[_animID].m_frames.size(); }
    bool GetAnimationLoop(int _animID) { return m_animations[_animID].m_loop; }
    Frame GetAnimationFrame(int _animID, int _frameID);
    void GetAnimationFrames(int _animID, vector<Frame>& _frames);
    int GetTilesetCount() { return m_tilesets.size(); }
    int GetTilesetPixelCount(int _tilesetID) { return m_tilesets[_tilesetID].m_data.size() * 2; }
    void GetTilesetPixels(int _tilesetID, vector<uint8_t>& _data);
    void GetAllPaletteGroups(vector<PaletteGroup>& _paletteGroups);
    void ReplaceAllPaletteGroups(vector<PaletteGroup> const& _paletteGroups);

    // Animation functions
    int NewAnimation(int _copyFrom = -1);
    void SwapAnimations(int _id1, int _id2);
    void DeleteAnimation(int _animID);
    void SetAnimationLoop(int _animID, bool _loop);

    // Frame functions
    int NewFrame(int _animID, int _copyAnimID = -1, int _copyFrameID = -1);
    void SwapFrames(int _animID, int _id1, int _id2);
    void DeleteFrame(int _animID, int _frameID);
    void ReplaceFrame(int _animID, int _frameID, Frame const& _frame);

    // Tileset functions
    bool ImportTileset(int _tilesetID, wstring const& _fileName, string& _errorMsg);
    bool ExportTileset(int _tilesetID, wstring const& _fileName, string& _errorMsg);

    // Static Functions
    static uint32_t GBAtoRGB(uint16_t _color);
    static uint16_t RGBtoGBA(uint32_t _color);
    static uint32_t ClampRGB(uint32_t _color);

    // Custom Sprite functions
    void ImportCustomTileset(vector<uint8_t> const& _data);
    void ImportCustomFrame();

private:
    // Reading from bytes
    uint8_t ReadByte(FILE* _file);
    uint16_t ReadShort(FILE* _file);
    uint32_t ReadInt(FILE* _file);

    // Writing bytes
    void WriteByte(FILE* _file, uint8_t _writeByte);
    void WriteShort(FILE* _file, uint16_t _writeShort);
    void WriteInt(FILE* _file, uint32_t _writeInt);
    void AlignFourBytes(FILE* _file, bool _padExtra = false);

    string GetAddressString(uint32_t _address);

private:
    bool m_loaded;

    vector<Animation> m_animations;
    vector<Tileset> m_tilesets;
    vector<PaletteGroup> m_paletteGroups;
};

#endif // BNSPRITE_H
