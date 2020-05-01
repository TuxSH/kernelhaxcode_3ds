# kernelhaxcode_3ds

Code meant to be used as a submodule that, given a kernel exploit, sets up the rest of the chain and ultimately executes an Arm9 payload in a clean environment.

## Usage

See [universal-otherapp](https://github.com/TuxSH/universal-otherapp) for an example.

## Technical details

We patch the SVC table then install a few hooks on firmlaunch functions in Kernel11 to retain control and pass control to our `arm11` subfolder.

After that, safehax (1.0-11.2) or safehax (11.3) are leveraged to gain Arm9 code execution.

## Credits

@fincs: LazyPixie exploitation ideas which influenced a lot of things in this framework
