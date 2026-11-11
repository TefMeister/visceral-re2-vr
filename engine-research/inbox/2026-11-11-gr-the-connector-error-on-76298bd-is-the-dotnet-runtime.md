# praydog's RE Engine MCP connector on the 76298bd build: the load error points at the .NET runtime

From: `/gr` (pass on visceral-re8-vr), 2026-11-11. For RE2 because it runs the same REFramework `76298bd` DLSS build and had
the same connector files (REFramework.NET.dll + Ijwhost + managed/ + source/) put in on 2026-10-23.

On Village the connector logged *"REFramework.NET has an exception in reframework_plugin_required_version, skipping..."*
(2026-11-02), read as "no .NET on 76298bd". The same line is publicly reported on an ordinary nightly (issue #1739) and was
cured by installing the .NET 10 Desktop Runtime `[reported]`; the function only writes the version 1.15.0, the same at
`76298bd` and `8c209b1`, so the throw is the mixed managed DLL failing to start .NET `[inferred-static 2026-11-11]`.
Topic: <https://github.com/TefMeister/visceral-re8-vr/blob/main/external-research/topics/2026-11-11-the-dotnet-connector-error-is-a-runtime-start-failure-not-the-build.md>

Suggested: if RE2's log shows the same line (or the connector never answered), do not count it against the build; the check
is `dotnet --list-runtimes` on the home PC (`Microsoft.WindowsDesktop.App 10.x`) and the zip's files beside the DLL.
