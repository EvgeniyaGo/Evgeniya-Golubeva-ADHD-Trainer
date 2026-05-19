import { AppFrame } from "../components/layout/AppFrame";
import { CubePreview } from "../components/CubePreview";
import { GameModeCard } from "../components/GameModeCard";

export default function MainPage() {
  return (
    <AppFrame
      ariaLabel="Main"
      activePage="home"
    >
      <div className="main-home">
        <section className="main-hero" aria-labelledby="main-page-title">
          <div className="main-cube-wrap">
            <CubePreview />
          </div>

          <div className="main-hero-copy">
            <h1 id="main-page-title" className="main-title">
              Choose a training mode
            </h1>

            <p className="main-copy">
              The cube reacts to movement and helps train concentration,
              reaction, and memory. Results are saved in your profile.
            </p>
          </div>
        </section>

        <section className="main-modes" aria-label="Training modes">
          <div className="game-mode-grid">
            <GameModeCard
              title="Go/No-Go"
              subtitle="Train impulse control and response inhibition"
              label="Focus + self-control"
              icon="play"
              to="/games/go-no-go"
            />
            <GameModeCard
              title="3D Snake"
              subtitle="Train spatial planning and sustained attention"
              label="Planning + movement"
              icon="snake"
              to="/games/snake"
            />
          </div>
        </section>

        <p className="main-help-text">Select a mode to configure the session.</p>
      </div>
    </AppFrame>
  );
}
