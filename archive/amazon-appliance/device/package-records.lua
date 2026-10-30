-- Retire old apps and detach reader ownership; never run old hooks.
local sql=require('lua-ljsqlite3/init')
local lfs=require('libs/libkoreader-lfs')
local dbpath='/mnt/us/kmc/kpm/kpm.db'
local quarantine=assert(os.getenv('GLE_QUARANTINE'))
assert(quarantine:match('^/mnt/us/gleworks%-quarantine%-%d+%-%d+$'), 'Unexpected quarantine path')
local db=sql.open(dbpath,'rw')
db:set_busy_timeout(1000)
if arg[1]=='--restore' then
    local backup=quarantine..'/kpm-before.sqlite3'
    assert(lfs.attributes(backup,'mode')=='file','Missing database snapshot')
    db:exec("ATTACH DATABASE '"..backup.."' AS original; PRAGMA foreign_keys=ON; BEGIN IMMEDIATE")
    local ok,err=pcall(function()
        local present=db:rowexec("SELECT count(*) FROM main.repositories WHERE id='unlocked'")
        if present==0 then
            assert(db:rowexec("SELECT count(*) FROM main.installed_packages WHERE id IN ('ku-http','ku-tailscale','koreader')")==0,'Old package IDs were reused')
            -- Restore only removed rows; preserve all unrelated package-manager changes.
            db:exec("INSERT INTO main.repositories SELECT * FROM original.repositories WHERE id='unlocked'")
            db:exec("INSERT INTO main.packages SELECT * FROM original.packages WHERE repository='unlocked'")
            db:exec("INSERT INTO main.artifacts SELECT * FROM original.artifacts WHERE repository='unlocked'")
            db:exec("INSERT INTO main.artifact_dependencies SELECT * FROM original.artifact_dependencies WHERE artifact_repository='unlocked'")
            db:exec("INSERT INTO main.installed_packages SELECT * FROM original.installed_packages WHERE id IN ('ku-http','ku-tailscale','koreader')")
            db:exec("INSERT INTO main.current_dependencies SELECT * FROM original.current_dependencies WHERE dependent IN ('ku-http','ku-tailscale','koreader') OR dependency_id IN ('ku-http','ku-tailscale','koreader')")
        else
            -- An interrupted removal either committed entirely or changed nothing.
            for _,t in ipairs({'repositories','installed_packages'}) do
                local where=t=='repositories' and "id='unlocked'" or "id IN ('ku-http','ku-tailscale','koreader')"
                assert(db:rowexec('SELECT count(*) FROM (SELECT * FROM main.'..t..' WHERE '..where..' EXCEPT SELECT * FROM original.'..t..' WHERE '..where..')')==0,'Old package records changed; manual merge required')
                assert(db:rowexec('SELECT count(*) FROM (SELECT * FROM original.'..t..' WHERE '..where..' EXCEPT SELECT * FROM main.'..t..' WHERE '..where..')')==0,'Old package records incomplete; manual merge required')
            end
        end
        assert(db:rowexec('SELECT count(*) FROM pragma_foreign_key_check')==0)
        db:exec('COMMIT')
    end)
    if not ok then pcall(function() db:exec('ROLLBACK') end); db:close(); error(err) end
    db:close(); print('Original app records restored without replacing unrelated records'); return
end
assert(db:rowexec("SELECT count(*) FROM installed_packages WHERE id='koreader' AND repository='kindlemodding'")==1,'Reader package ownership changed')
local repo=db:rowexec("SELECT id FROM repositories WHERE url='https://kindle.trinh.uk/r'")
assert(repo=='unlocked','Repository identity changed; review before migrating')
assert(db:rowexec("SELECT count(*) FROM installed_packages WHERE repository='unlocked' AND id NOT IN ('ku-http','ku-tailscale')")==0,'Other apps use this repository')
assert(db:rowexec("SELECT count(*) FROM current_dependencies WHERE dependency_id IN ('ku-http','ku-tailscale','koreader') AND dependent NOT IN ('ku-http','ku-tailscale','koreader')")==0,'Other installed apps depend on the old apps')
assert(db:rowexec("SELECT count(*) FROM installed_packages WHERE id IN ('ku-http','ku-tailscale') AND repository<>'unlocked'")==0,'Package ownership changed')
if arg[1]=='--check' then db:close(); print('Package ownership and dependencies checked'); return end
assert(arg[1]=='--remove','Expected --check or --remove')
local backup=quarantine..'/kpm-before.sqlite3'
assert(not lfs.symlinkattributes(backup),'Database backup already exists')
-- SQLite creates a consistent snapshot, including any WAL pages.
db:exec("VACUUM INTO '"..backup.."'")
local marker=assert(io.open(quarantine..'/db-removal-started','wb')); assert(marker:write('Selective rollback uses kpm-before.sqlite3\n')); assert(marker:close())
local ffi=require('ffi'); ffi.cdef[[void sync(void);]]
ffi.C.sync() -- Persist the recovery snapshot/marker before committing removals.
db:exec('PRAGMA foreign_keys=ON; BEGIN IMMEDIATE')
local ok, err=pcall(function()
    -- Recheck under the write lock to avoid deleting concurrent installs.
    assert(db:rowexec("SELECT count(*) FROM installed_packages WHERE repository='unlocked' AND id NOT IN ('ku-http','ku-tailscale')")==0)
    assert(db:rowexec("SELECT count(*) FROM current_dependencies WHERE dependency_id IN ('ku-http','ku-tailscale','koreader') AND dependent NOT IN ('ku-http','ku-tailscale','koreader')")==0)
    db:exec("DELETE FROM installed_packages WHERE id IN ('ku-http','ku-tailscale') AND repository='unlocked'")
    db:exec("DELETE FROM installed_packages WHERE id='koreader' AND repository='kindlemodding'")
    db:exec("DELETE FROM repositories WHERE id='unlocked' AND url='https://kindle.trinh.uk/r'")
    assert(db:rowexec('SELECT count(*) FROM pragma_foreign_key_check')==0)
    db:exec('COMMIT')
end)
if not ok then pcall(function() db:exec('ROLLBACK') end); db:close(); error(err) end
db:exec('PRAGMA wal_checkpoint(TRUNCATE)')
db:close()
print('Old app registrations/repository removed; reader detached from KPM; KMC preserved')
