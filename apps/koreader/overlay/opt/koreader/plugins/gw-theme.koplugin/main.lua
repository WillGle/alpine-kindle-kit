-- SPDX-License-Identifier: GPL-3.0-or-later
-- Gleworks theme adapter for KOReader.
--
-- Scope: reader chrome only. The footer is styled automatically while the
-- theme is enabled; reading typography is a deliberate one-shot action so a
-- theme cannot silently rewrite every book's layout.

local WidgetContainer = require("ui/widget/container/widgetcontainer")
local Event = require("ui/event")
local Notification = require("ui/widget/notification")
local UIManager = require("ui/uimanager")
local _ = require("gettext")
local Theme = require("gw_theme")

local GwTheme = WidgetContainer:extend{
    name = "gw_theme",
    is_doc_only = true,
}

local FOOTER_KEYS = {
    "text_font_face",
    "text_font_size",
    "text_font_bold",
    "container_height",
    "container_bottom_padding",
    "progress_style_thin",
    "align",
}

local function enabled()
    return G_reader_settings:nilOrTrue("gw_theme_enabled")
end

function GwTheme:init()
    if self.ui.menu then
        self.ui.menu:registerToMainMenu(self)
    end
end

function GwTheme:applyFooter()
    local footer = self.ui and self.ui.footer
    if not footer or not footer.settings or not footer.footer_text then
        return
    end
    if not enabled() then
        self:restoreFooter()
        return
    end

    local settings = footer.settings
    if not self._saved_footer then
        self._saved_footer = {}
        for _, key in ipairs(FOOTER_KEYS) do
            self._saved_footer[key] = settings[key]
        end
    end

    settings.text_font_face = Theme.font.ui
    settings.text_font_size = Theme.reader.footer_font_size
    settings.text_font_bold = false
    settings.container_height = Theme.reader.footer_height
    settings.container_bottom_padding = Theme.reader.footer_bottom_padding
    settings.progress_style_thin = true
    settings.align = "center"

    footer:updateFooterFont()
    footer:refreshFooter(true, true)
end

function GwTheme:restoreFooter()
    local footer = self.ui and self.ui.footer
    if not footer or not footer.settings or not footer.footer_text then
        return
    end
    if not self._saved_footer then
        return
    end
    for key, value in pairs(self._saved_footer) do
        footer.settings[key] = value
    end
    self._saved_footer = nil
    footer:updateFooterFont()
    footer:refreshFooter(true, true)
end

function GwTheme:applyTypography()
    -- These are KOReader units, not physical pixels. The values in
    -- theme/tokens.json are the design intent; this keeps them close without
    -- pretending KOReader's typography is CSS.
    self.ui:handleEvent(Event:new("SetFontSize", Theme.reader.font_size))
    self.ui:handleEvent(Event:new("SetLineSpace", Theme.reader.line_spacing))
    self.ui:handleEvent(Event:new("SetPageMargins", {
        Theme.reader.page_margin_lr,
        Theme.reader.page_margin_tb,
        Theme.reader.page_margin_lr,
        Theme.reader.page_margin_tb,
    }))
    Notification:notify(_("Gleworks reading typography applied"))
end

function GwTheme:addToMainMenu(menu_items)
    menu_items.gw_theme = {
        text = _("Gleworks theme"),
        sub_item_table = {
            {
                text = _("Enable theme"),
                checked_func = enabled,
                callback = function()
                    G_reader_settings:flipNilOrTrue("gw_theme_enabled")
                    if enabled() then
                        self:applyFooter()
                    else
                        self:restoreFooter()
                    end
                    UIManager:setDirty(self.ui, "ui")
                end,
            },
            {
                text = _("Apply reading typography"),
                callback = function()
                    self:applyTypography()
                end,
            },
        },
    }
end

function GwTheme:onReaderReady()
    -- Re-apply after the document is ready; footer text and page count now
    -- exist, so the restyle does not race the first render.
    self:applyFooter()
end

return GwTheme
