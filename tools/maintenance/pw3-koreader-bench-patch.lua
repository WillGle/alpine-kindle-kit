-- Test-only KOReader user patch for Phase 3 cache experiments.
-- Install as /opt/koreader/patches/2-bench-exit.lua, set
-- GW_BENCH_EXIT_AFTER=<seconds>, then remove it after the experiment.
--
-- This performs a normal ReaderUI close (which flushes CRE cache) instead of
-- killing the process, and logs cache state without touching book content.
local exit_after = tonumber(os.getenv("GW_BENCH_EXIT_AFTER") or "")
if not exit_after or exit_after <= 0 then return end

local ReaderUI = require("apps/reader/readerui")
local UIManager = require("ui/uimanager")
local logger = require("logger")

local orig_init = ReaderUI.init
function ReaderUI:init(...)
    orig_init(self, ...)
    UIManager:scheduleIn(exit_after, function()
        if ReaderUI.instance ~= self or self.document == nil then return end
        local doc = self.document
        logger.info("BENCH_CLOSE cache=", doc:getCacheFilePath() or "none",
                    " has=", tostring(doc:hasCacheFile()),
                    " stale=", tostring(doc:isCacheFileStale()))
        self:onClose(false)
        UIManager:quit()
    end)
end
