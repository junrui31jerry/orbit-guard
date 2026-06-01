import { useEffect, useRef } from "react";
import * as THREE from "three";

export default function EarthView() {
  const mountRef = useRef<HTMLDivElement | null>(null);

  useEffect(() => {
    const mount = mountRef.current;
    if (!mount) return;

    const height = 360;
    const width = mount.clientWidth || 640;
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 1000);
    camera.position.set(0, 0, 7);

    if (typeof window.WebGLRenderingContext === "undefined") {
      mount.dataset.webgl = "unavailable";
      return;
    }

    let renderer: THREE.WebGLRenderer;
    try {
      renderer = new THREE.WebGLRenderer({ antialias: true });
    } catch {
      mount.dataset.webgl = "unavailable";
      return;
    }

    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.setSize(width, height);
    renderer.setClearColor(0x08121b);
    mount.appendChild(renderer.domElement);

    const earth = new THREE.Mesh(
      new THREE.SphereGeometry(1.8, 48, 32),
      new THREE.MeshBasicMaterial({ color: 0x2d74b8 }),
    );
    scene.add(earth);

    const leoShell = new THREE.LineLoop(
      new THREE.BufferGeometry().setFromPoints(
        Array.from({ length: 160 }, (_, index) => {
          const angle = (Math.PI * 2 * index) / 160;
          return new THREE.Vector3(Math.cos(angle) * 2.6, Math.sin(angle) * 2.6, 0);
        }),
      ),
      new THREE.LineBasicMaterial({ color: 0x56c6ff }),
    );
    scene.add(leoShell);

    const inclinedShell = leoShell.clone();
    inclinedShell.rotation.x = Math.PI / 3.4;
    scene.add(inclinedShell);

    let animationFrame = 0;

    const render = () => {
      earth.rotation.y += 0.005;
      leoShell.rotation.z += 0.0025;
      inclinedShell.rotation.z -= 0.002;
      renderer.render(scene, camera);
      animationFrame = window.requestAnimationFrame(render);
    };

    render();

    return () => {
      window.cancelAnimationFrame(animationFrame);
      earth.geometry.dispose();
      (earth.material as THREE.Material).dispose();
      leoShell.geometry.dispose();
      (leoShell.material as THREE.Material).dispose();
      renderer.dispose();
      mount.replaceChildren();
    };
  }, []);

  return <div className="earth-view" ref={mountRef} aria-label="3D Earth event view" />;
}
