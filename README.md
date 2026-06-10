# Desarrollo de Firmware para ESP32

Proyecto académico enfocado en el desarrollo de firmware desde cero para la ESP32, implementando una arquitectura por capas basada en Drivers, HAL y BSP mediante acceso directo a registros.

## Desarrolladores

- Jorge Habib Hernandez Rojas
- Hector Rodrigo Teruel Grado
-Diego Alejandro Saenz Duarte
## Objetivos

- Comprender la arquitectura interna de la ESP32.
- Implementar drivers mediante acceso a registros.
- Diseñar una capa HAL (Hardware Abstraction Layer).
- Diseñar una capa BSP (Board Support Package).
- Documentar el proceso de desarrollo.

## Arquitectura del Proyecto

Application
│
├── BSP
│
├── HAL
│
├── Drivers
│ ├── GPIO
│ ├── TIMER
│
└── Hardware 


## Convención de Ramas

- `main`: versión estable.
- `develop`: integración de funcionalidades.
- `feature/*`: nuevas características.
- `feature/bugfix/*`: corrección de errores.



