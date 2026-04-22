You must add the following code to your `init.lua`:

```lua
local buffer = import("micro/buffer")
local shell = import("micro/shell")

function vivifyOpen(buf)
    local cursor = buf:GetActiveCursor()
    local line = cursor.Loc.Y + 1

    shell.ExecCommand("viv", string.format("%s:%d", buf.AbsPath, line))
end

function onBufferOpen(buf)
    if os.getenv("MICRO_VIVIFY") ~= "1" then
        return
    end

    if buf.Type.Kind == buffer.BTDefault then
        vivifyOpen(buf)
    end
end
```

This enables Vivify to run automatically when a file is opened, effectively simulating “startup” behavior (since Micro does not provide a true startup command hook for this use case).

Big thanks to @Andriamanitra for answering [my questions](https://github.com/micro-editor/micro/discussions/4078).

