Name:           flintc
Version:        0.4.1
Release:        1%{?dist}
Summary:        Flint programming language compiler and language server

License:        MIT
URL:            https://github.com/flint-lang/flintc
Source0:        https://github.com/flint-lang/flintc/releases/download/v%{version}-core/flintc
Source1:        https://github.com/flint-lang/flintc/releases/download/v%{version}-core/fls

%description
Flint is a high-level, transparent programming language.

%prep
# No preparation, the binareis are published directly

%build
# No build needed, we ship prebuilt static binaries

%install
install -Dm755 %{SOURCE0} %{buildroot}%{_bindir}/flintc
install -Dm755 %{SOURCE1} %{buildroot}%{_bindir}/fls

%files
%{_bindir}/flintc
%{_bindir}/fls

%changelog
* Fri Aug 28 2026 Marc Zweiler marc.zweiler@outlook.at - 0.4.1-1
- Changelog at https://github.com/flint-lang/flintc/releases/tag/v0.4.1-core
* Thu Jul 9 2026 Marc Zweiler marc.zweiler@outlook.at - 0.4.0-1
- Changelog at https://github.com/flint-lang/flintc/releases/tag/v0.4.0-core
* Wed Jun 10 2026 Marc Zweiler marc.zweiler@outlook.at - 0.3.5-1
- Initial release for COPR
