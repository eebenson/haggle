# Introduction #

There are two major branches in the haggle repository. This page will try to explain why, and what the differences are.

# default #

The default branch is the development branch of haggle. This branch is where active development happens.

# haggle-demo #

This branch is used for demos with haggle, which is why it contains some extra things. In this branch there are some changes made specifically to make demoing haggle, to people who don't know what haggle is, a bit more easy. To this end, an additional manager has been inserted, called the "Vendetta Manager", which communicates with a program called "Vendetta". There is a copy of Vendetta included in this branch, so you can run demos. The demo video shown on the [project home page](http://code.google.com/p/haggle/) is showing the demo branch of haggle running on the phones, and Vendetta running on a computer.

This branch is meant to be fairly up-to-date with the default branch. Therefore the changes to the default branch is often merged into the haggle-demo branch, to keep it up to date. The changes to the haggle-demo branch should **_never_** be merged into the default branch, because the default branch should not contain the demo-specific changes in the haggle-demo branch.

# Other branches #

What you do in your cloned copy of the haggle repository, is your business. But please don't push any new branches into the googlecode repository without permission. The goal here is to have as few branches as possible. Right now that means we have one development branch, and one demo branch. We currently see no need for any further branches, but that could change.