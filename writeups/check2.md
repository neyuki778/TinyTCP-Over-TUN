Checkpoint 2 Writeup
====================

My name: [your name here]

My SUNet ID: [your sunetid here]

I collaborated with: [list sunetids here]

I would like to thank/reward these classmates for their help: [list sunetids here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

Describe Wrap32 and TCPReceiver structure and design. [Describe data
structures and approach taken. Describe alternative designs considered
or tested.  Describe benefits and weaknesses of your design compared
with alternatives -- perhaps in terms of simplicity/complexity, risk
of bugs, asymptotic performance, empirical performance, required
implementation time and difficulty, and other factors. Include any
measurements if applicable.]

Implementation Challenges:
1. during the implementation of wrapping func, I used bitwise operation in a pratical problem for the first time
2. during the debugging of tcp-receiver, I realized why the instructor designed wrapping and unwrapping—to convert the 32-bit TCP sequence number (seqno) into a 64-bit absolute sequence number (abs-seqno) to be passed to the reassembler.

Remaining Bugs:
1. unwrap: candidate > checkpoint and candidate - checkpoint > TWO_POW_31, and when candidate > TWO_POW_32 means that candidate is too far away from checkpoint
2. tcp-receive: message.seqno should be converted into a 64bit abs-seqno(starts from 0), that it can be used in insertion
3. tcp-send: windsize should less than UINT64_MAX
4. tcp-send: flags do affect the length of abs-seqno

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]

- Optional: I made an extra test I think will be helpful in catching bugs: [describe where to find]
