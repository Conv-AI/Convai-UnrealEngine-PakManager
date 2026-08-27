> **Status: superseded by ADR-0009.**

# The Pak Manager visits the SDK shell; it does not move in

The Pak Manager stays its own plugin with its own release, and reaches the Convai editor shell by
registering a page into it rather than by being absorbed into it. A reader who sees this plugin
depending on the SDK's widgets, styling and shell will reasonably ask why it is not simply part of
the SDK — this is why.

The audiences differ by an order of magnitude. The SDK ships to every Convai Unreal developer; the
Pak Manager serves only creators who generated a project with the Modding Tool. Absorbing it would
link the packaging toolchain, the live-coding and Blutility dependencies and the dependency-copy API
into every Convai game, for a panel almost none of them will open — and it would widen this
Windows-only, cross-compile-dependent tool to a plugin that also supports Mac and Android.

It would also trade a compile-time version floor for a release-cadence coupling, which is the worse
of the two: a floor is checked once at build, whereas coupling is felt on every fix, with a hotfix
waiting behind an SDK release and its marketplace review. Independent movement is the same reason
the Job System ships alongside rather than inside — see ADR-0002.

## Consequences

Registering a page needs a route added to the SDK, because its route type is a closed enum and its
factory registration is therefore an internal composition seam rather than a third-party extension
point. That is four additive lines across two SDK files — the enum value and its two string
conversions, plus an entry in the protected-route set — against moving an entire plugin into it.

The SDK's header bar builds its navigation from a hard-coded list rather than from the registered
routes, so a route alone puts no button anywhere. Rather than make the SDK's navigation conditional
on whether this plugin is installed, the Pak Manager keeps its own entry point and navigates into
the shell itself. Every line that knows this plugin exists then lives in this plugin, and the SDK
carries a route it never has to reason about.

Living in the shell means living behind the SDK's sign-in. That is a gain, not a cost: it replaces
the API key currently held in plaintext in the creator's project — and uploaded inside the Raw
Project Archive — with credential handling this plugin does not have to write.
