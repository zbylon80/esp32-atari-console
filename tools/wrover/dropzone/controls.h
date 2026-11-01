#pragma once
// The frontend inverts these bits when Atari reads its active-low joystick.
// Opposite directions cancel independently on each axis.
constexpr int drop_joystick(bool left,bool right,bool up,bool down) {
    return (left&&!right?4:0)|(right&&!left?8:0)|(up&&!down?1:0)|(down&&!up?2:0);
}
