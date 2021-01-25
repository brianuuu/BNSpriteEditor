#include "palettecontextmenu.h"

PaletteContextMenu::PaletteContextMenu(QWidget *parent) : QMenu(parent)
{
    copyColorAction = new QAction("Copy Color", this);
    replaceColorAction = new QAction("Replace Color", this);
    copyPaletteAction = new QAction("Copy Palette", this);
    replacePaletteAction = new QAction("Replace Palette", this);
    insertAboveAction = new QAction("Insert Above", this);
    insertBelowAction = new QAction("Insert Below", this);
    deletePaletteAction = new QAction("Delete Palette", this);

    this->addAction(copyColorAction);
    this->addAction(replaceColorAction);
    this->addSeparator();
    this->addAction(copyPaletteAction);
    this->addAction(replacePaletteAction);
    this->addAction(insertAboveAction);
    this->addAction(insertBelowAction);
    this->addAction(deletePaletteAction);

    reset();
}

void PaletteContextMenu::reset()
{
    replaceColorAction->setEnabled(false);
    replacePaletteAction->setEnabled(false);
    insertAboveAction->setEnabled(false);
    insertBelowAction->setEnabled(false);

    m_colorIndex = -1;
    m_paletteIndex = -1;
    m_colorCopied = 0;
    m_paletteCopied.clear();
    m_colorPressed = 0;
    m_palettePressed.clear();
}

void PaletteContextMenu::setColorCopied()
{
    // TODO: Copy to clipboard
    m_colorCopied = m_colorPressed;
    replaceColorAction->setEnabled((m_colorCopied >> 24) == 0xFF);
}

void PaletteContextMenu::setPaletteCopied()
{
    // TODO: Copy to clipboard
    m_paletteCopied = m_palettePressed;

    bool enabled = m_paletteCopied.size() > 0;
    replacePaletteAction->setEnabled(enabled);
    insertAboveAction->setEnabled(enabled);
    insertBelowAction->setEnabled(enabled);
}
