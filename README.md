# Sony Vaio Model Lock Patcher
Tool to remove BIOS/Model lock from old (Win 98/Me/2000/XP era) Sony Vaio recovery media.

It should work on pretty much all Sony recovery discs that uses BLITTER format.

## How to use it
This tool removes locks only from *.IMG files (those are located in disc root folder).

Keep in mind that some earlier recovery discs (typically Win 98/Me) also have extra lock mechanism that need to be disabled separately (more on this [here](#removing-extra-model-locks-from-earlier-recovery-discs))

Step by step instruction:

1. Open your recovery `CD/ISO` with some tool. The built-in Windows tool will work for now, but later you will need to replace modified files inside the `ISO`. So, find a tool that allows `ISO` modification.
2. Copy all `*.IMG` files to some folder. They are located in CD root folder and named `SONY.IMG` or `NTFS.IMG`.
3. Also copy `*.I01`, `*.I02` files from disc `#2`, `#3` and so on to the same folder.
4. Create somewhere another folder that will be used to save patched files.
5. Download latest release, extract exe and open it.
6. Press `Open` button and select main file: `SONY.IMG` or `NTFS.IMG`
7. Now you should see some image information, like:
    1. `Date time` with date and time when image was created. Some very old images doesn't have time so it might show 01-01-1970 and that's ok.
    2. `Image version` shows the restore image version. I am aware of only three versions: `5F5B5465`, `606C6576`, and `4E4A4354`. The tool currently supports only the first two. If you see version `4E4A4354` or any other version not listed here, DO NOT continue further—please create an issue with a download link for that recovery CD and I will try to take a look.
    3. `Model locks` this will show on what model the image is locked, if you see empty list, this means that image is not model locked (or it's some kind of unknown lock version) and you don't need to continue.
8. Press `Select` button and chose folder you created at step #4
9. Press `Remove locks` button. If recovery has more that `1 discs`, once disc one patched you will be asked to select next image part, choose `SONY.I01` when you asked for disc `1`, `SONY.I02` for disc `2` and so on. Wait until you see operation finished message. 
10. If your recovery also has `NTFS.IMG`, repeat steps 6 and 9 again, but this time open `NTFS.IMG` instead of `SONY.IMG`.
11. Now replace original files inside `ISO` with patched ones from folder created at step #4
12. Burn CD (better try with CD-RW first) and recover your beautiful Vaio to its original state.

### Removing extra model locks from earlier recovery discs

Some particularly old recovery CD has some extra model lock protection.
I know only 2 types, but there could be more.

Type 1: Extra lock in `INSTALL4.EXE`, located in the `SONY` subfolder. If you have this file, get a HEX editor like `HxD`, open `INSTALL4.EXE` in that editor, and find the string `PCG-`. There should be only one occurrence of this string. You should see something like `PCG-C1VN*(I*`. Enter the BIOS on your Vaio and note the `Machine Name`, then replace the model letters in the EXE to match what you see in the BIOS. For example, if in the BIOS you see `PCG-C1PVK(UC)`, change `PCG-C1VN*(I*` to `PCG-C1VP*(U*`. Replacing non-matching characters with `*` should also work, as `*` means any character. VERY IMPORTANT: DO NOT ADD OR REMOVE characters, just replace them, so the size of the modified EXE must be exactly the same. After this, replace `INSTALL4.EXE` in your ISO (in all discs) and you should be good to go.

Type 2: Extra lock using `bioslock.exe`. Open you ISO file with some ISO editor tool like WinImage, extract BOOT floppy image from it. Then open that BOOT image. There you should see folders for different languages (ENG, GER, etc). Inside these folders find `RECOVER.BAT`. Open `RECOVER.BAT` in any text editor, then find `bioslock` command. It will look something like this:
        
        ```
        ECHO [2J
        %RAMD%:\bioslock %MODELNAME%
        if errorlevel 1 goto BAD_COMP
        if errorlevel 0 goto Welcome
        ```
Check value for `errorlevel 0` (`goto Welcome` in this case) and add string `goto Welcome` before that text block so it will look like this:

        ```
        goto Welcome
        ECHO [2J
        %RAMD%:\bioslock %MODELNAME%
        if errorlevel 1 goto BAD_COMP
        if errorlevel 0 goto Welcome
        ```
Save the file, replace it in BOOT floppy image, replace original BOOT image with modified one and save ISO.

## Disclaimer

Do not try to restore your Vaio with a recovery disc from a completely different model. Instead, find a recovery CD for a laptop from your series and as close as possible in terms of specifications.

All downloads and software are provided 'as is' without warranty of any kind. The download and use of any materials are done at your own discretion and risk, and you will be solely responsible for any damage to your computer system or loss of data that results from such activities.
