**NOTE**: The current implementation is a proof-of-concept when it comes to security. The implementation aims to show how the security principles and mechanisms discussed below fit into the architecture and how they can be implemented. There is no claim that the current implementation provides adequate security.

# Introduction #

Security is important for any network architecture. As Haggle is an experimental research project, we do not claim that the architecture and implementation are completely secure. Still, security is an integral part of the Haggle architecture and there are a number of principles that has guided our design. The goal with these principles is to outline a path towards a complete security framework. Below, you will find a discussion of these principles and the motivation behind them.

# Content-Centric Security #

As Haggle is a content-centric network architecture, its security framework naturally requires some rethinking of established security principles. Haggle aims to secure content rather than hosts, and therefore:

  * Nodes can choose to reject content and, when they do so, the decision is based on the trust in the content (or the content creator) rather than the node the content is received from.

This principle follows from the fact that Haggle nodes rely on each other to forward content. There is no _always-available-infrastructure_ that nodes can rely on for pushing bits between each other. Haggle (as a distributed network) simply does not have the luxury to reject content from nodes that are not trusted, because it would seriously hurt the dissemination of content. Therefore, a node may happily accept content from untrusted (or rather unauthenticated nodes), as long as the content can be authenticated. Similarly, a node does not avoid disseminating content to untrusted nodes, as it relies on such nodes to disseminate its content to interested parties. From this follows:

  * If a node does not want to reveal sensitive content or information to a third-party, the node should encrypt this data rather than refraining from disseminating to the third-party.

The above principle should hold little controversy. As in the Internet, one does not refrain from sending information to, e.g., an Internet bank, just because there are untrusted intermediary routers. Encryption is the right tool in these types of situations.

Both the above principles can easily be implemented with a standard security toolbox and, in particular, public-key cryptography. As with most applications of these tools, key distribution is a major issue---ever more so in a decentralized network such as Haggle. Currently, Haggle does not mandate a specific mechanism for distributing keys and therefore a number of approaches can be used, such as pre-installing keys, over-the-air distribution using infrastructure when available, PGP-style webs-of-trust, etc.

# Control-Information Security #

Although encryption and authentication of content is quite straight-forward, the encryption of control-information is not. In Haggle, dissemination is based on revealing one's interests so that they can be matched against content in the network. This opens up privacy issues for those that do not want to unnecessarily reveal such information. However, it is inevitable that some of this information has to be revealed for dissemination to work. Inventing mechanisms to allow content-matching against interests with minimal privacy implications is a major challenge. A goal of the Haggle project is to develop algorithms that allow the following:

  * Nodes can encrypt their sensitive information in their node descriptions (e.g., their interests).
  * Interests can be "secretly matched" against content on another node by only revealing how well a content item matches an interest list, without revealing exactly which interests match and what those interests are.

The development of algorithms that allow the above is currently under work.

# Security Implementation #

The Haggle implementation currently supports the signing and verification of content.

Nodes embed their own certificate in, e.g., their node description and disseminate it to other nodes. A certificate is typically signed by a central authority that vouches for the node in the network. The private key associated with the certificate is used by the node to sign all the content it creates.

Those that receive certificates verify them against the central authority and if the certificate is trusted, it is stored in a local certificate repository. As with content, nodes do not care how they acquire certificates of other nodes, as long as they can verify its signature against a trusted authority.

The public keys embedded in certificates are used to verify that the received content has been created and signed by trusted nodes. Trusted content is accepted while untrusted content is rejected. To reduce the overhead of rejections, only the metadata header of the content data object has to be received for a reject or accept. This is possible as the metadata contains the signature and the hash of the data. A data object can therefore be rejected before its payload is received.

Complete rejection of untrusted content may seriously hurt the dissemination and forwarding of content. For example, nodes cannot act as delegate forwarders (content carriers without their own interest in the content) without having first received the node description (and hence certificate) of the content source. Therefore, Haggle implements three security levels that allow different node behavior:

  1. **LOW**: No security -- all content is accepted.
  1. **MEDIUM**: Accept all content data objects, but only signed (and verifiable) node descriptions.
  1. **HIGH**: Only accept data objects which are signed and verifiable (includes both content and node descriptions).

Using the MEDIUM setting (current default), nodes may forward data objects although they cannot themselves verify them. This setting does not hurt the dissemination of content in the network too much, but still allows those that actually want to use the content to verify it if necessary.

Haggle does not implement encryption of data object payload (i.e., the content). The reason for this is that such encryption should probably be implemented by applications. Haggle disseminates data objects and their payloads without looking at it and therefore cares little whether the bits in the payload are encrypted or not.