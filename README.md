# Minitar

The goal of this project is to develop a simplified version of the tar utility, called minitar. tar is one of the popular tools among users of Linux and other Unix-derived operating systems. It creates a single archive file from several member files. Thus, tar is very similar to the zip tool we use for code submission in this class, although it does not perform file compression as zip does.

The tar utility was originally developed back in the era of widespread archival storage on magnetic tapes – tar is in fact short for “tape archive.” Therefore, archive files created by tar have a very simple format that is easy to read and write sequentially. While we won’t require you to replicate the full functionality of the original tar utility, your minitar program will still be quite capable. It will allow you to create to create archives from files, modify existing archives, and extract the original files back from an archive.

minitar is fully Posix-compliant, meaning it can freely interoperate with all of the standard tape-archive utility programs like the original tar. In other words, minitar will be able to read and manipulate archives generated by tar, and vice versa. In fact, we will be testing your implementation for equivalence with a subset of the standard tar features.
