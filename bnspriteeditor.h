#ifndef BNSPRITECREATOR_H
#define BNSPRITECREATOR_H

#include <QColorDialog>
#include <QDebug>
#include <QFileDialog>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMessageBox>
#include <QScrollBar>
#include <QTreeWidgetItem>
#include <QtCore>
#include <QtGui>

#include "bnsprite.h"
#include "customspritemanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class BNSpriteEditor; }
QT_END_NAMESPACE

typedef QVector<QRgb> Palette;
typedef QVector<Palette> PaletteGroup;

class BNSpriteEditor : public QMainWindow
{
    Q_OBJECT

public:
    BNSpriteEditor(QWidget *parent = nullptr);
    ~BNSpriteEditor();

    bool eventFilter(QObject *object, QEvent *event);
    void closeEvent (QCloseEvent *event);

private slots:
    // Actions
    void on_actionImport_Sprite_triggered();
    void on_actionImport_SF_Sprite_triggered();
    void on_actionExport_Sprite_triggered();
    void on_actionExport_SF_Sprite_triggered();
    void on_actionClose_triggered();
    void on_actionSimple_Mode_triggered();
    void on_actionAdvanced_Mode_triggered();
    void on_actionMerge_Sprite_triggered();
    void on_actionExport_Sprite_as_Single_PNG_triggered();
    void on_actionExport_Sprite_as_Individual_PNG_triggered();
    void on_actionAbout_BNSpriteEditor_triggered();
    void on_actionAbout_Qt_triggered();
    void on_actionCustom_Sprite_Manager_triggered();

    // Animation
    void on_Anim_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_Anim_PB_Up_clicked();
    void on_Anim_PB_Down_clicked();
    void on_Anim_PB_New_clicked();
    void on_Anim_PB_Dup_clicked();
    void on_Anim_PB_Del_clicked();

    // Frame
    void on_Frame_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_Frame_CB_Loop_toggled(bool checked);
    void on_Frame_PB_Play_clicked();
    void on_Frame_Play_timeout();
    void on_Frame_PB_ShiftL_clicked();
    void on_Frame_PB_ShiftR_clicked();
    void on_Frame_PB_New_clicked();
    void on_Frame_PB_Copy_clicked();
    void on_Frame_PB_Paste_clicked();
    void on_Frame_PB_Del_clicked();
    void on_Frame_SB_Delay_valueChanged(int arg1);
    void on_Frame_CB_Flag0_toggled(bool checked);
    void on_Frame_CB_Flag1_toggled(bool checked);

    // Tileset
    void on_Tileset_SB_Index_valueChanged(int arg1);
    void on_Tileset_PB_Import_clicked();
    void on_Tileset_PB_Export_clicked();

    // Sub Animation
    void on_SubAnim_Tabs_currentChanged(int index);
    void on_SubAnim_PB_New_clicked();
    void on_SubAnim_PB_Dup_clicked();
    void on_SubAnim_PB_Del_clicked();
    void on_SubFrame_PB_Play_clicked();
    void on_SubFrame_Play_timeout();
    void on_SubFrame_CB_Loop_toggled(bool checked);
    void on_SubFrame_LW_currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous);
    void on_SubFrame_PB_Add_clicked();
    void on_SubFrame_PB_Del_clicked();
    void on_SubFrame_PB_ShiftL_clicked();
    void on_SubFrame_PB_ShiftR_clicked();
    void on_SubFrame_SB_Index_valueChanged(int arg1);
    void on_SubFrame_SB_Delay_valueChanged(int arg1);

    // Palette
    void on_Palette_SB_Group_valueChanged(int arg1);
    void on_Palette_SB_Index_valueChanged(int arg1);
    void on_Palette_PB_New_clicked();
    void on_Palette_PB_Del_clicked();
    void on_Palette_Color_changed(int paletteIndex, int colorIndex, QRgb color);

    // Object
    void on_Object_Tabs_currentChanged(int index);
    void on_Object_PB_New_clicked();
    void on_Object_PB_Dup_clicked();
    void on_Object_PB_Del_clicked();
    void on_OAM_TW_currentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous);
    void on_OAM_PB_Up_clicked();
    void on_OAM_PB_Down_clicked();
    void on_OAM_PB_New_clicked();
    void on_OAM_PB_Dup_clicked();
    void on_OAM_PB_Del_clicked();
    void on_OAM_SB_Tile_valueChanged(int arg1);
    void on_OAM_CB_Size_currentTextChanged(const QString &arg1);
    void on_OAM_SB_XPos_valueChanged(int arg1);
    void on_OAM_SB_YPos_valueChanged(int arg1);
    void on_OAM_CB_HFlip_toggled(bool checked);
    void on_OAM_CB_VFlip_toggled(bool checked);
    void on_OAM_Highlight_timeout();

    // Preview
    void on_Preview_BG_currentTextChanged(const QString &arg1);
    void on_Preview_CB_Highlight_clicked(bool checked);
    void on_Preview_pressed(QPoint pos);

    // Image control
    void on_Image_PB_Up_pressed();
    void on_Image_PB_Up_released();
    void on_Image_PB_Down_pressed();
    void on_Image_PB_Down_released();
    void on_Image_PB_Left_pressed();
    void on_Image_PB_Left_released();
    void on_Image_PB_Right_pressed();
    void on_Image_PB_Right_released();
    void on_Image_PB_HFlip_clicked();
    void on_Image_PB_VFlip_clicked();
    void on_Image_Move_timeout();

    // Custom Sprite
    void on_CSM_BuildSprite_pressed(int tilesetCount);
    void on_CSM_BuildCheckButton_pressed();
    void on_CSM_BuildPushFrame_pressed(int frameID, bool newAnim);
    void on_CSM_LoadProject_pressed(QString file, uint32_t fileVersion, qint64 skipByte, int tilesetCount);
    void on_CSM_SaveProject_pressed(QString file);

private:
    void ResetProgram();
    void ResetAnim();
    void ResetFrame();
    void ResetOthers();
    void ResetOAM();
    void ResetSubFrame();

    void ImportSprite(bool isSFSprite);
    void ExportSprite(bool isSFSprite);

    // Thumbnails
    QImage* GetFrameImage(BNSprite::Frame const& _frame, bool _isThumbnail, int _subAnimID = 0, int _subFrameID = 0);
    void UpdateFrameImage(BNSprite::Frame const& _frame, QImage* _image, int32_t _minX = -128, int32_t _minY = -128, int _subAnimID = 0, int _subFrameID = 0);
    void DrawOAMInImage(BNSprite::SubObject const& _subObject, QImage* _image, int32_t _minX, int32_t _minY, vector<uint8_t> const& _data, bool _drawFirstColor);

    // Animation
    void AddAnimationThumbnail(int _animID);
    void UpdateAnimationThumnail(int _animID);
    void SwapAnimation(int _oldID, int _newID);

    // Frame
    void AddFrameThumbnail(int _animID, int _frameID);
    void UpdateFrameThumbnail(int _frameID);
    void SwapFrame(int _animID, int _oldID, int _newID, bool _updateAnimThumbnail);

    // Tileset
    void CacheTileset();
    void UpdateDrawTileset();

    // Sub Animation
    void AddSubFrame(int _subFrameID);
    void SwapSubFrame(int _oldID, int _newID);
    void UpdateSubFrameThumbnail(int _subFrameID);
    void UpdateSubFrameThumbnailFromObject(int _objectID);

    // OAM
    void AddOAM(BNSprite::SubObject const& _subObject);
    void SwapOAM(int _oldID, int _newID);
    void UpdateOAMThumbnail(BNSprite::SubObject const& _subObject);

    // Preview
    void AddPreviewOAM(BNSprite::SubObject const& _subObject);
    void DrawPreviewOAM(QGraphicsPixmapItem* _graphicsItem, BNSprite::SubObject const& _subObject);
    void UpdatePreviewOAM(int _oamID, BNSprite::SubObject const& _subObject, bool _redraw);
    void HighlightPreviewOAM();

    // Image Control
    void MoveImage();
    void FlipImage(bool _vertical);
    void UpdateImageControlButtons();

    // Palette
    void AddPaletteGroupFromSprite(BNSprite::PaletteGroup const& paletteGroup);
    void SetPaletteSelected(int index);
    void UpdatePalettePreview();

private:
    Ui::BNSpriteEditor *ui;
    QSettings *m_settings;
    bool m_simpleMode;
    CustomSpriteManager* m_csm = Q_NULLPTR;

    // File
    BNSprite m_sprite;
    BNSprite m_spriteMerge;
    QString m_path;

    // Thumbnails
    QVector<QImage*> m_animThumbnails;
    QVector<QImage*> m_frameThumbnails;

    // Current frame we're editing
    BNSprite::Frame m_frame;
    int m_copyAnim;
    int m_copyFrame;

    // Highlight OAM square
    QTimer* m_highlightTimer;
    bool m_highlightUp;
    int m_highlightAlpha;
    QGraphicsPixmapItem* m_previewHighlight;

    // Cached images
    vector<uint8_t> m_tilesetData;
    QGraphicsScene* m_tilesetGraphic;
    QGraphicsScene* m_oamGraphic;
    QGraphicsScene* m_previewGraphic;
    QGraphicsPixmapItem* m_previewBG;
    QVector<QGraphicsPixmapItem*> m_previewOAMs;
    QImage* m_tilesetImage;
    QImage* m_oamImage;

    // Image moving
    QTimer *m_moveTimer;
    int m_moveDir;

    // Play animation
    QTimer *m_animationTimer;
    bool m_playingAnimation;
    QTimer *m_subAnimationTimer;
    bool m_playingSubAnimation;
    int m_playingSubFrame;

    // Palette
    QVector<PaletteGroup> m_paletteGroups;
};
#endif // BNSPRITECREATOR_H
