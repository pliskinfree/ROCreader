# Legal Disclaimer and Content Policy

This document describes the legal and content policy for the official ROCreader
project materials. It is not legal advice.

## General Purpose

ROCreader is a general-purpose reader application. It is designed for local
files, personal libraries, public-domain works, open-license works, and
authorized online catalogs configured by the user.

The official project does not provide, host, index, recommend, or bundle:

- Copyrighted books, comics, manga, novels, images, or other works without
  authorization.
- Third-party source lists for unauthorized content.
- Illegal download links.
- Tools or instructions intended to bypass access controls for infringing
  downloads.
- Modified builds that use the official name or channels to distribute illegal
  content.

## Online Sources

The URL Entry module is a client-side feature. It reads user-provided
configuration from `online_sources.ini` and can connect to lawful OPDS/Kavita
catalogs or other lawful private sources.

Official releases must keep `online_sources.ini` template-only. Real source
entries are user/device configuration and must not be treated as official
project content.

The maintainers do not control user-configured sources and do not grant
permission to use ROCreader to access, copy, download, distribute, or make
available any content in violation of applicable law or third-party rights.

## User Responsibility

Users are responsible for ensuring that:

- They own or have permission to access every file they read.
- Their configured online catalogs are lawful.
- Any downloaded or cached content is legally obtained.
- Their use complies with local law and the terms of the relevant content
  provider.

## Contributor Rules

Contributors must not submit, link to, or document infringing or illegal
content. This includes pull requests, issues, discussions, wiki pages, release
notes, examples, screenshots, and test data.

Do not contribute source presets or adapters for sites whose apparent purpose is
unauthorized distribution of copyrighted works.

Do not use official project infrastructure to promote, troubleshoot, or improve
access to unauthorized content sources.

## Third-Party Forks and Modified Builds

Third-party forks, modified builds, source packs, and redistributed packages are
not official ROCreader releases unless explicitly published by the maintainer
through the official project repository.

The original authors and maintainers are not responsible for independent
third-party modifications, user-configured sources, or redistributed packages
that add illegal content, infringing source lists, or unlawful download
functionality without authorization.

Third-party distributors must not imply that illegal source lists, illegal
content packs, or infringing modified builds are endorsed by the official
project.

## Takedown and Abuse Reports

If official project materials contain content that you believe infringes your
rights or violates applicable law, please report it through the project issue
tracker or maintainer contact channel.

Please include:

- The exact URL, file path, release, issue, discussion, or commit.
- A description of the allegedly infringing or illegal material.
- Your relationship to the rights holder or affected party.
- Contact information for follow-up.

The maintainers will review valid reports and remove, disable, or clarify
official project materials where appropriate.

## Packaging Policy

Official binary packages should include only the application, required runtime
assets, templates, and files that the project has permission to distribute.

Release packages must not include real third-party content source lists or
copyrighted works unless the project has explicit authorization to distribute
them.

Online updates must preserve the user's local `online_sources.ini` because it is
user/device configuration, not official packaged content.
