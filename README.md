# PST File Format SDK in C#

This is the Microsoft PST SDK hosted by CodePlex here. The contents of the sub-directories are from the `pstsdk.zip` file available using the `download archive` button.

http://pstsdk.codeplex.com/

**Note: CodePlex End of Life**

CodePlex was moved into "Archive only" mode on December 15, 2017. This project has not yet been migrated to Microsoft's new location on GtiHub: https://github.com/Microsoft

https://blogs.msdn.microsoft.com/bharry/2017/03/31/shutting-down-codeplex/

CodePlex does not have a file viewer and the wiki appears to be offline so this mirror was created.

Some differences to date:

* This expands the `sourceCode` folder from a zip file so the contents can be browsed.

## Background

You can read about this SDK here:

https://www.infoq.com/news/2010/05/Outlook-PST-View-Tool-and-SDK

> Three months ago Microsoft released the Outlook PST Specification documentation allowing developers to create server/desktop applications processing PST content without having to install Outlook. On May 24th, Microsoft announced two new open source projects,  PST Data Structure View Tool and PST File Format SDK, making the creation of such applications even easier.
> 
> Developers could access PST file content through the Messaging API (MAPI) via the Outlook Object Model since 2007. In February 2010, Microsoft released the complete Outlook PST File Format Structure specification in an attempt to help interoperability with other document processing systems. (The details were presented by InfoQ at that time.) The recently released PST Data Structure View Tool and PST File Format SDK offer a starting point for writing PST applications without having to deal with internal details of PST files.
> 
> The PST Data Structure View Tool is a MFC/C++ graphical tool developers can use to understand the internal organization of data in PST files. The PST File Format SDK is a cross-platform C++ library which offers read access to all items stored in a PST file. Microsoft promises to add writing capabilities in the near future.
> 
> Microsoft explains that opening up PST helps interoperability for clients with heterogeneous document systems and those having large amounts of archived PST data. Now, they can search through the PST data, extract and process it in various ways.
> 
> Both projects have been released under the Apache license.

https://blogs.msdn.microsoft.com/usisvde/2010/06/11/pst-file-format-sdk-released/

Author: [John Wiese](https://social.msdn.microsoft.com/profile/John+Wiese)

> The Outlook team has announced the availability of the new PST File Format SDK.  The SDK is available at on codeplex: PST File Format SDK
> 
> You can read a little more at the Outlook team blog here: Announcing the PST File Format SDK

https://blogs.technet.microsoft.com/port25/2010/05/24/new-open-source-projects-foster-interop-with-outlook-pst-data-files/

Author: [Peter Galli](https://social.technet.microsoft.com/profile/Peter+Galli)

More information is availble here:

https://msdn.microsoft.com/en-us/library/ff385210.aspx